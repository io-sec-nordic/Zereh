#define _GNU_SOURCE

#include "codegen.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct zereh_sb {
    char *data;
    size_t len;
    size_t cap;
};

static int zereh_sb_init(struct zereh_sb *sb, size_t cap)
{
    sb->data = calloc(1, cap);
    if (!sb->data) {
        return -1;
    }
    sb->len = 0;
    sb->cap = cap;
    return 0;
}

static void zereh_sb_free(struct zereh_sb *sb)
{
    free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static int zereh_sb_reserve(struct zereh_sb *sb, size_t extra)
{
    size_t required = sb->len + extra + 1;
    size_t cap = sb->cap;
    char *next;

    if (required <= sb->cap) {
        return 0;
    }

    while (cap < required) {
        cap *= 2;
    }

    next = realloc(sb->data, cap);
    if (!next) {
        return -1;
    }

    sb->data = next;
    sb->cap = cap;
    return 0;
}

static int zereh_sb_appendn(struct zereh_sb *sb, const char *text, size_t n)
{
    if (zereh_sb_reserve(sb, n) != 0) {
        return -1;
    }

    memcpy(sb->data + sb->len, text, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
    return 0;
}

static int zereh_sb_appendf(struct zereh_sb *sb, const char *fmt, ...)
{
    va_list ap;
    va_list ap_copy;
    int need;

    va_start(ap, fmt);
    va_copy(ap_copy, ap);
    need = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);

    if (need < 0) {
        va_end(ap);
        return -1;
    }

    if (zereh_sb_reserve(sb, (size_t)need) != 0) {
        va_end(ap);
        return -1;
    }

    vsnprintf(sb->data + sb->len, sb->cap - sb->len, fmt, ap);
    va_end(ap);
    sb->len += (size_t)need;
    return 0;
}

static int zereh_read_file(const char *path, char **content)
{
    FILE *fp = NULL;
    long size;
    char *buf = NULL;

    *content = NULL;

    fp = fopen(path, "rb");
    if (!fp) {
        perror("fopen template");
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }

    size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return -1;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    buf = calloc(1, (size_t)size + 1);
    if (!buf) {
        fclose(fp);
        return -1;
    }

    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        return -1;
    }

    fclose(fp);
    *content = buf;
    return 0;
}

static int zereh_write_file(const char *path, const char *content)
{
    FILE *fp = fopen(path, "wb");
    size_t len;

    if (!fp) {
        perror("fopen generated_source");
        return -1;
    }

    len = strlen(content);
    if (fwrite(content, 1, len, fp) != len) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

static char *zereh_escape_c_string(const char *text)
{
    struct zereh_sb sb;
    size_t i;

    if (!text) {
        return strdup("");
    }

    if (zereh_sb_init(&sb, 64) != 0) {
        return NULL;
    }

    for (i = 0; text[i] != '\0'; i++) {
        char c = text[i];

        if (c == '\\' || c == '"') {
            if (zereh_sb_appendf(&sb, "\\%c", c) != 0) {
                zereh_sb_free(&sb);
                return NULL;
            }
        } else if (c == '\n') {
            if (zereh_sb_appendn(&sb, "\\n", 2) != 0) {
                zereh_sb_free(&sb);
                return NULL;
            }
        } else if (c == '\r') {
            if (zereh_sb_appendn(&sb, "\\r", 2) != 0) {
                zereh_sb_free(&sb);
                return NULL;
            }
        } else if (c == '\t') {
            if (zereh_sb_appendn(&sb, "\\t", 2) != 0) {
                zereh_sb_free(&sb);
                return NULL;
            }
        } else {
            if (zereh_sb_appendn(&sb, &c, 1) != 0) {
                zereh_sb_free(&sb);
                return NULL;
            }
        }
    }

    return sb.data;
}

static int zereh_replace_all(const char *input,
                             const char *token,
                             const char *replacement,
                             char **output)
{
    const char *cursor = input;
    const char *hit;
    size_t token_len = strlen(token);
    struct zereh_sb sb;

    *output = NULL;

    if (!input || !token || !replacement || token_len == 0) {
        return -1;
    }

    if (zereh_sb_init(&sb, strlen(input) + strlen(replacement) + 256) != 0) {
        return -1;
    }

    while ((hit = strstr(cursor, token)) != NULL) {
        if (zereh_sb_appendn(&sb, cursor, (size_t)(hit - cursor)) != 0) {
            zereh_sb_free(&sb);
            return -1;
        }

        if (zereh_sb_appendn(&sb, replacement, strlen(replacement)) != 0) {
            zereh_sb_free(&sb);
            return -1;
        }

        cursor = hit + token_len;
    }

    if (zereh_sb_appendn(&sb, cursor, strlen(cursor)) != 0) {
        zereh_sb_free(&sb);
        return -1;
    }

    *output = sb.data;
    return 0;
}

static int zereh_append_route_case(struct zereh_sb *sb, const struct zereh_route *route, uint32_t route_id)
{
    char *prefix = NULL;
    char *suffix = NULL;
    char *action = NULL;
    int rc = -1;

    prefix = zereh_escape_c_string(route->prefix);
    suffix = zereh_escape_c_string(route->suffix);
    if (!prefix || !suffix) {
        goto out;
    }

    switch (route->target) {
    case ZEREH_TARGET_USER_SPACE:
        if (asprintf(&action,
                     "        return bpf_redirect_map(&xsk_map, %u, 0);\n",
                     route->xsk_queue) < 0) {
            action = NULL;
            goto out;
        }
        break;
    case ZEREH_TARGET_KERNEL_SPACE:
        if (asprintf(&action,
                     "        return zereh_route_kernel_space(iph, udp, bpf_htons(%u), 0);\n",
                     route->kernel_port) < 0) {
            action = NULL;
            goto out;
        }
        break;
    case ZEREH_TARGET_XDP_PROG:
        if (asprintf(&action,
                     "        bpf_tail_call(ctx, &prog_array, %u);\n"
                     "        return XDP_DROP;\n",
                     route->prog_index) < 0) {
            action = NULL;
            goto out;
        }
        break;
    default:
        goto out;
    }

    if (zereh_sb_appendf(sb,
                         "    case 0x%016llxULL:\n"
                         "        if (!zereh_apply_app_filters(qmeta,\n"
                         "                                     \"%s\",\n"
                         "                                     %zu,\n"
                         "                                     \"%s\",\n"
                         "                                     %zu,\n"
                         "                                     %u,\n"
                         "                                     %u,\n"
                         "                                     %u,\n"
                         "                                     %u,\n"
                         "                                     route_hash,\n"
                         "                                     %u,\n"
                         "                                     0x%xu,\n"
                         "                                     %u,\n"
                         "                                     %u,\n"
                         "                                     %u,\n"
                         "                                     %u,\n"
                         "                                     %u,\n"
                         "                                     %u,\n"
                         "                                     %u)) {\n"
                         "            return XDP_DROP;\n"
                         "        }\n"
                         "%s",
                         (unsigned long long)route->hash,
                         prefix,
                         strlen(route->prefix),
                         suffix,
                         strlen(route->suffix),
                         route->min_labels,
                         route->max_labels,
                         route->qname_min_len,
                         route->qname_max_len,
                         route_id,
                         route->rrtype_mask,
                         route->pow_difficulty,
                         route->pow_time_window,
                         route->bloom_enabled,
                         route->bloom_size,
                         route->bloom_hash_functions,
                         route->bloom_time_window,
                         route->rate_limit_qps,
                         action) != 0) {
        goto out;
    }

    rc = 0;
out:
    free(prefix);
    free(suffix);
    free(action);
    return rc;
}

static int zereh_append_fallback_rule(struct zereh_sb *sb, const struct zereh_route *route, uint32_t route_id)
{
    char *domain = NULL;
    char *prefix = NULL;
    char *suffix = NULL;
    char *action = NULL;
    int rc = -1;

    domain = zereh_escape_c_string(route->domain);
    prefix = zereh_escape_c_string(route->prefix);
    suffix = zereh_escape_c_string(route->suffix);
    if (!domain || !prefix || !suffix) {
        goto out;
    }

    switch (route->target) {
    case ZEREH_TARGET_USER_SPACE:
        if (asprintf(&action,
                     "            return bpf_redirect_map(&xsk_map, %u, 0);\n",
                     route->xsk_queue) < 0) {
            action = NULL;
            goto out;
        }
        break;
    case ZEREH_TARGET_KERNEL_SPACE:
        if (asprintf(&action,
                     "            return zereh_route_kernel_space(iph, udp, bpf_htons(%u), 0);\n",
                     route->kernel_port) < 0) {
            action = NULL;
            goto out;
        }
        break;
    case ZEREH_TARGET_XDP_PROG:
        if (asprintf(&action,
                     "            bpf_tail_call(ctx, &prog_array, %u);\n"
                     "            return XDP_DROP;\n",
                     route->prog_index) < 0) {
            action = NULL;
            goto out;
        }
        break;
    default:
        goto out;
    }

    if (zereh_sb_appendf(sb,
                         "    if (zereh_match_suffix(qmeta->qname, qmeta->qname_len, \"%s\", %zu)) {\n"
                         "        if (!zereh_apply_app_filters(qmeta,\n"
                         "                                     \"%s\",\n"
                         "                                     %zu,\n"
                         "                                     \"%s\",\n"
                         "                                     %zu,\n"
                         "                                     %u,\n"
                         "                                     %u,\n"
                         "                                     %u,\n"
                         "                                     %u,\n"
                         "                                     route_hash,\n"
                         "                                     %u,\n"
                         "                                     0x%xu,\n"
                         "                                     %u,\n"
                         "                                     %u,\n"
                         "                                     %u,\n"
                         "                                     %u,\n"
                         "                                     %u,\n"
                         "                                     %u,\n"
                         "                                     %u)) {\n"
                         "            return XDP_DROP;\n"
                         "        }\n"
                         "%s"
                         "    }\n",
                         domain,
                         strlen(route->domain),
                         prefix,
                         strlen(route->prefix),
                         suffix,
                         strlen(route->suffix),
                         route->min_labels,
                         route->max_labels,
                         route->qname_min_len,
                         route->qname_max_len,
                         route_id,
                         route->rrtype_mask,
                         route->pow_difficulty,
                         route->pow_time_window,
                         route->bloom_enabled,
                         route->bloom_size,
                         route->bloom_hash_functions,
                         route->bloom_time_window,
                         route->rate_limit_qps,
                         action) != 0) {
        goto out;
    }

    rc = 0;
out:
    free(domain);
    free(prefix);
    free(suffix);
    free(action);
    return rc;
}

static int zereh_generate_route_cases(const struct zereh_config *cfg, char **out)
{
    struct zereh_sb sb;
    uint32_t i;

    *out = NULL;

    if (zereh_sb_init(&sb, 2048) != 0) {
        return -1;
    }

    if (zereh_sb_appendf(&sb,
                         "    /* Generated from YAML: %u routes, %u blacklist CIDRs */\n",
                         cfg->route_count,
                         cfg->blacklist_count) != 0) {
        zereh_sb_free(&sb);
        return -1;
    }

    for (i = 0; i < cfg->route_count; i++) {
        if (zereh_append_route_case(&sb, &cfg->routes[i], i) != 0) {
            zereh_sb_free(&sb);
            return -1;
        }
    }

    *out = sb.data;
    return 0;
}

static int zereh_generate_fallback_rules(const struct zereh_config *cfg, char **out)
{
    struct zereh_sb sb;
    uint32_t i;

    *out = NULL;

    if (zereh_sb_init(&sb, 1024) != 0) {
        return -1;
    }

    for (i = 0; i < cfg->route_count; i++) {
        if (zereh_append_fallback_rule(&sb, &cfg->routes[i], i) != 0) {
            zereh_sb_free(&sb);
            return -1;
        }
    }

    *out = sb.data;
    return 0;
}

int zereh_generate_xdp_source(const struct zereh_config *cfg)
{
    char *template_text = NULL;
    char *route_cases = NULL;
    char *fallback_rules = NULL;
    char packet_size_str[32];
    char opcode_mask_str[32];
    char drop_truncated_str[8];
    char inline_checksums_str[8];
    char hash_algo_str[8];
    char rate_state_entries_str[16];
    const char *default_action_str;
    char *tmp1 = NULL;
    char *tmp2 = NULL;
    char *tmp3 = NULL;
    char *tmp4 = NULL;
    char *tmp5 = NULL;
    char *tmp6 = NULL;
    char *tmp7 = NULL;
    char *tmp8 = NULL;
    char *tmp9 = NULL;
    int rc = -1;

    if (!cfg) {
        return -1;
    }

    if (zereh_read_file(cfg->template_path, &template_text) != 0) {
        fprintf(stderr, "failed to read template: %s\n", cfg->template_path);
        return -1;
    }

    if (zereh_generate_route_cases(cfg, &route_cases) != 0) {
        fprintf(stderr, "failed generating route switch cases\n");
        goto out;
    }

    if (zereh_generate_fallback_rules(cfg, &fallback_rules) != 0) {
        fprintf(stderr, "failed generating route fallback rules\n");
        goto out;
    }

    snprintf(packet_size_str, sizeof(packet_size_str), "%u", cfg->max_packet_size);
    snprintf(opcode_mask_str, sizeof(opcode_mask_str), "%u", cfg->allowed_opcode_mask);
    snprintf(drop_truncated_str, sizeof(drop_truncated_str), "%u", cfg->drop_truncated ? 1u : 0u);
    snprintf(inline_checksums_str, sizeof(inline_checksums_str), "%u", cfg->inline_checksums ? 1u : 0u);
    snprintf(hash_algo_str, sizeof(hash_algo_str), "%u", cfg->hash_algo == ZEREH_HASH_SIPHASH ? 1u : 0u);
    snprintf(rate_state_entries_str,
             sizeof(rate_state_entries_str),
             "%u",
             cfg->route_count ? cfg->route_count : 1u);
    default_action_str = (cfg->default_action == ZEREH_ACTION_DROP) ? "XDP_DROP" : "XDP_PASS";

    if (zereh_replace_all(template_text,
                          "/*__ZEREH_ROUTE_CASES__*/",
                          route_cases,
                          &tmp1) != 0) {
        goto out;
    }

    if (zereh_replace_all(tmp1,
                          "/*__ZEREH_FALLBACK_RULES__*/",
                          fallback_rules,
                          &tmp2) != 0) {
        goto out;
    }

    if (zereh_replace_all(tmp2,
                          "__ZEREH_MAX_PACKET_SIZE__",
                          packet_size_str,
                          &tmp3) != 0) {
        goto out;
    }

    if (zereh_replace_all(tmp3,
                          "__ZEREH_ALLOWED_OPCODE_MASK__",
                          opcode_mask_str,
                          &tmp4) != 0) {
        goto out;
    }

    if (zereh_replace_all(tmp4,
                          "__ZEREH_DROP_TRUNCATED__",
                          drop_truncated_str,
                          &tmp5) != 0) {
        goto out;
    }

    if (zereh_replace_all(tmp5,
                          "__ZEREH_DEFAULT_ACTION__",
                          default_action_str,
                          &tmp6) != 0) {
        goto out;
    }

    if (zereh_replace_all(tmp6,
                          "__ZEREH_INLINE_CHECKSUMS__",
                          inline_checksums_str,
                          &tmp7) != 0) {
        goto out;
    }

    if (zereh_replace_all(tmp7,
                          "__ZEREH_HASH_ALGO__",
                          hash_algo_str,
                          &tmp8) != 0) {
        goto out;
    }

    if (zereh_replace_all(tmp8,
                          "__ZEREH_RATE_STATE_ENTRIES__",
                          rate_state_entries_str,
                          &tmp9) != 0) {
        goto out;
    }

    if (zereh_write_file(cfg->generated_source, tmp9) != 0) {
        fprintf(stderr, "failed writing generated source: %s\n", cfg->generated_source);
        goto out;
    }

    rc = 0;
out:
    free(template_text);
    free(route_cases);
    free(fallback_rules);
    free(tmp1);
    free(tmp2);
    free(tmp3);
    free(tmp4);
    free(tmp5);
    free(tmp6);
    free(tmp7);
    free(tmp8);
    free(tmp9);
    return rc;
}
