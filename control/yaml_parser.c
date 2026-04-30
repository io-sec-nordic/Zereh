#include "yaml_parser.h"

#include "config.h"
#include "hash.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#if __has_include(<yaml.h>)
#include <yaml.h>
#define ZEREH_HAVE_LIBYAML 1
#elif __has_include(<yaml-0.1/yaml.h>)
#include <yaml-0.1/yaml.h>
#define ZEREH_HAVE_LIBYAML 1
#else
#define ZEREH_HAVE_LIBYAML 0
#endif

#if !ZEREH_HAVE_LIBYAML

int zereh_parse_config_yaml(const char *path, struct zereh_config *cfg)
{
    (void)path;
    (void)cfg;
    fprintf(stderr, "libyaml headers are not available; install libyaml development package\n");
    return -1;
}

#else

static yaml_node_t *zereh_map_get(yaml_document_t *doc, yaml_node_t *map, const char *key)
{
    yaml_node_pair_t *pair;
    size_t key_len;

    if (!doc || !map || map->type != YAML_MAPPING_NODE || !key) {
        return NULL;
    }

    key_len = strlen(key);

    for (pair = map->data.mapping.pairs.start; pair < map->data.mapping.pairs.top; pair++) {
        yaml_node_t *k = yaml_document_get_node(doc, pair->key);
        yaml_node_t *v = yaml_document_get_node(doc, pair->value);

        if (!k || k->type != YAML_SCALAR_NODE || !v) {
            continue;
        }

        if (k->data.scalar.length == key_len &&
            memcmp((const char *)k->data.scalar.value, key, key_len) == 0) {
            return v;
        }
    }

    return NULL;
}

static const char *zereh_scalar(yaml_node_t *node)
{
    if (!node || node->type != YAML_SCALAR_NODE) {
        return NULL;
    }
    return (const char *)node->data.scalar.value;
}

static int zereh_parse_u32(yaml_node_t *node, uint32_t *out)
{
    const char *s = zereh_scalar(node);
    char *end = NULL;
    unsigned long v;

    if (!s || !out) {
        return -1;
    }

    v = strtoul(s, &end, 10);
    if (end == s || *end != '\0' || v > 0xffffffffUL) {
        return -1;
    }

    *out = (uint32_t)v;
    return 0;
}

static int zereh_parse_u16(yaml_node_t *node, uint16_t *out)
{
    uint32_t tmp = 0;
    if (zereh_parse_u32(node, &tmp) != 0 || tmp > 0xffffU) {
        return -1;
    }
    *out = (uint16_t)tmp;
    return 0;
}

static int zereh_parse_u8(yaml_node_t *node, uint8_t *out)
{
    uint32_t tmp = 0;
    if (zereh_parse_u32(node, &tmp) != 0 || tmp > 0xffU) {
        return -1;
    }
    *out = (uint8_t)tmp;
    return 0;
}

static int zereh_parse_bool(yaml_node_t *node, uint8_t *out)
{
    const char *s = zereh_scalar(node);

    if (!s || !out) {
        return -1;
    }

    if (strcasecmp(s, "true") == 0 || strcasecmp(s, "yes") == 0 ||
        strcasecmp(s, "on") == 0 || strcmp(s, "1") == 0) {
        *out = 1;
        return 0;
    }

    if (strcasecmp(s, "false") == 0 || strcasecmp(s, "no") == 0 ||
        strcasecmp(s, "off") == 0 || strcmp(s, "0") == 0) {
        *out = 0;
        return 0;
    }

    return -1;
}

static uint32_t zereh_rrtype_to_mask(const char *rr)
{
    if (!rr) {
        return 0;
    }
    if (strcasecmp(rr, "A") == 0) {
        return ZEREH_RRTYPE_A;
    }
    if (strcasecmp(rr, "AAAA") == 0) {
        return ZEREH_RRTYPE_AAAA;
    }
    if (strcasecmp(rr, "TXT") == 0) {
        return ZEREH_RRTYPE_TXT;
    }
    if (strcasecmp(rr, "ANY") == 0) {
        return ZEREH_RRTYPE_ANY;
    }
    return 0;
}

static int zereh_parse_target(const char *target, enum zereh_target_type *out)
{
    if (!target || !out) {
        return -1;
    }
    if (strcasecmp(target, "user_space") == 0) {
        *out = ZEREH_TARGET_USER_SPACE;
        return 0;
    }
    if (strcasecmp(target, "kernel_space") == 0) {
        *out = ZEREH_TARGET_KERNEL_SPACE;
        return 0;
    }
    if (strcasecmp(target, "xdp_prog") == 0) {
        *out = ZEREH_TARGET_XDP_PROG;
        return 0;
    }
    return -1;
}

static int zereh_parse_default_action(yaml_node_t *node, enum zereh_default_action *out)
{
    const char *s = zereh_scalar(node);

    if (!s || !out) {
        return -1;
    }

    if (strcasecmp(s, "pass") == 0 || strcasecmp(s, "xdp_pass") == 0) {
        *out = ZEREH_ACTION_PASS;
        return 0;
    }

    if (strcasecmp(s, "drop") == 0 || strcasecmp(s, "xdp_drop") == 0) {
        *out = ZEREH_ACTION_DROP;
        return 0;
    }

    return -1;
}

static int zereh_parse_xdp_mode(yaml_node_t *node, enum zereh_xdp_mode *out)
{
    const char *s = zereh_scalar(node);

    if (!s || !out) {
        return -1;
    }

    if (strcasecmp(s, "native") == 0 || strcasecmp(s, "drv") == 0) {
        *out = ZEREH_XDP_MODE_NATIVE;
        return 0;
    }

    if (strcasecmp(s, "skb") == 0 || strcasecmp(s, "generic") == 0) {
        *out = ZEREH_XDP_MODE_SKB;
        return 0;
    }

    if (strcasecmp(s, "offload") == 0 || strcasecmp(s, "hw") == 0) {
        *out = ZEREH_XDP_MODE_OFFLOAD;
        return 0;
    }

    return -1;
}

static int zereh_parse_hash_algorithm(yaml_node_t *node, struct zereh_config *cfg)
{
    const char *s = zereh_scalar(node);

    if (!s || !cfg) {
        return -1;
    }

    if (strcasecmp(s, "FNV-1a") == 0 ||
        strcasecmp(s, "fnv1a") == 0 ||
        strcasecmp(s, "fnv-1a") == 0) {
        cfg->hash_algo = ZEREH_HASH_FNV1A;
        snprintf(cfg->hash_algorithm, sizeof(cfg->hash_algorithm), "%s", "FNV-1a");
        return 0;
    }

    if (strcasecmp(s, "SipHash") == 0 ||
        strcasecmp(s, "siphash") == 0 ||
        strcasecmp(s, "siphash24") == 0 ||
        strcasecmp(s, "siphash-2-4") == 0) {
        cfg->hash_algo = ZEREH_HASH_SIPHASH;
        snprintf(cfg->hash_algorithm, sizeof(cfg->hash_algorithm), "%s", "SipHash");
        return 0;
    }

    return -1;
}

static int zereh_parse_rrtype_mask(yaml_document_t *doc, yaml_node_t *node, uint32_t *out_mask)
{
    yaml_node_item_t *it;

    if (!doc || !node || !out_mask) {
        return -1;
    }

    if (node->type == YAML_SCALAR_NODE) {
        uint32_t mask = zereh_rrtype_to_mask(zereh_scalar(node));
        if (!mask) {
            return -1;
        }
        *out_mask = mask;
        return 0;
    }

    if (node->type != YAML_SEQUENCE_NODE) {
        return -1;
    }

    *out_mask = 0;
    for (it = node->data.sequence.items.start; it < node->data.sequence.items.top; it++) {
        yaml_node_t *entry = yaml_document_get_node(doc, *it);
        uint32_t mask = zereh_rrtype_to_mask(zereh_scalar(entry));
        if (!mask) {
            return -1;
        }
        *out_mask |= mask;
    }

    return *out_mask ? 0 : -1;
}

static int zereh_parse_allowed_opcodes(yaml_document_t *doc, yaml_node_t *node, struct zereh_config *cfg)
{
    yaml_node_item_t *it;

    if (!node || !cfg) {
        return -1;
    }

    cfg->allowed_opcode_mask = 0;

    if (node->type == YAML_SCALAR_NODE) {
        uint32_t opcode = 0;
        if (zereh_parse_u32(node, &opcode) != 0 || opcode > 15U) {
            return -1;
        }
        cfg->allowed_opcode_mask |= (uint16_t)(1u << opcode);
        return 0;
    }

    if (node->type != YAML_SEQUENCE_NODE) {
        return -1;
    }

    for (it = node->data.sequence.items.start; it < node->data.sequence.items.top; it++) {
        yaml_node_t *entry = yaml_document_get_node(doc, *it);
        uint32_t opcode = 0;

        if (!entry || zereh_parse_u32(entry, &opcode) != 0 || opcode > 15U) {
            return -1;
        }
        cfg->allowed_opcode_mask |= (uint16_t)(1u << opcode);
    }

    if (cfg->allowed_opcode_mask == 0) {
        cfg->allowed_opcode_mask = (uint16_t)(1u << 0);
    }

    return 0;
}

static int zereh_parse_blacklist(yaml_document_t *doc, yaml_node_t *node, struct zereh_config *cfg)
{
    yaml_node_item_t *it;

    if (!node || !cfg || node->type != YAML_SEQUENCE_NODE) {
        return -1;
    }

    cfg->blacklist_count = 0;

    for (it = node->data.sequence.items.start; it < node->data.sequence.items.top; it++) {
        yaml_node_t *entry = yaml_document_get_node(doc, *it);
        const char *cidr = NULL;

        if (cfg->blacklist_count >= ZEREH_MAX_BLACKLIST) {
            fprintf(stderr, "blacklist exceeds max entries (%u)\n", ZEREH_MAX_BLACKLIST);
            return -1;
        }

        if (!entry) {
            continue;
        }

        if (entry->type == YAML_SCALAR_NODE) {
            cidr = zereh_scalar(entry);
        } else if (entry->type == YAML_MAPPING_NODE) {
            yaml_node_t *cidr_node = zereh_map_get(doc, entry, "cidr");
            cidr = zereh_scalar(cidr_node);
        }

        if (!cidr || cidr[0] == '\0') {
            return -1;
        }

        snprintf(cfg->blacklist[cfg->blacklist_count].cidr,
                 sizeof(cfg->blacklist[cfg->blacklist_count].cidr),
                 "%s",
                 cidr);
        cfg->blacklist_count++;
    }

    return 0;
}

static int zereh_parse_route_dns_types(yaml_document_t *doc, yaml_node_t *dns_types, struct zereh_route *route)
{
    yaml_node_t *allow_node;
    yaml_node_t *deny_node;
    uint32_t allow_mask = route->rrtype_mask;
    uint32_t deny_mask = 0;

    if (!doc || !dns_types || !route || dns_types->type != YAML_MAPPING_NODE) {
        return -1;
    }

    allow_node = zereh_map_get(doc, dns_types, "allow");
    if (allow_node && zereh_parse_rrtype_mask(doc, allow_node, &allow_mask) != 0) {
        return -1;
    }

    deny_node = zereh_map_get(doc, dns_types, "deny");
    if (deny_node && zereh_parse_rrtype_mask(doc, deny_node, &deny_mask) != 0) {
        return -1;
    }

    route->rrtype_mask = allow_mask & ~deny_mask;
    return route->rrtype_mask ? 0 : -1;
}

static int zereh_parse_bloom_seeds(yaml_document_t *doc, yaml_node_t *node, struct zereh_route *route)
{
    yaml_node_item_t *it;

    if (!doc || !node || !route || node->type != YAML_SEQUENCE_NODE) {
        return -1;
    }

    route->bloom_seed_count = 0;

    for (it = node->data.sequence.items.start; it < node->data.sequence.items.top; it++) {
        yaml_node_t *entry = yaml_document_get_node(doc, *it);
        const char *seed;

        if (route->bloom_seed_count >= ZEREH_MAX_BLOOM_SEEDS) {
            fprintf(stderr,
                    "route %s bloom seeds exceed max (%u)\n",
                    route->domain,
                    ZEREH_MAX_BLOOM_SEEDS);
            return -1;
        }

        seed = zereh_scalar(entry);
        if (!seed || seed[0] == '\0') {
            return -1;
        }

        snprintf(route->bloom_seeds[route->bloom_seed_count],
                 sizeof(route->bloom_seeds[route->bloom_seed_count]),
                 "%s",
                 seed);
        route->bloom_seed_count++;
    }

    return 0;
}

static int zereh_parse_route_filters(yaml_document_t *doc,
                                     yaml_node_t *filters,
                                     struct zereh_route *route,
                                     int *has_min_labels,
                                     int *has_max_labels,
                                     int *has_min_length,
                                     int *has_max_length)
{
    yaml_node_t *node;

    if (!doc || !filters || !route || filters->type != YAML_MAPPING_NODE) {
        return -1;
    }

    node = zereh_map_get(doc, filters, "pow");
    if (node && node->type == YAML_MAPPING_NODE) {
        yaml_node_t *v;

        v = zereh_map_get(doc, node, "enabled");
        if (v && zereh_parse_bool(v, &route->pow_enabled) != 0) {
            return -1;
        }

        v = zereh_map_get(doc, node, "difficulty");
        if (v && zereh_parse_u8(v, &route->pow_difficulty) != 0) {
            return -1;
        }

        v = zereh_map_get(doc, node, "time_window");
        if (v && zereh_parse_u16(v, &route->pow_time_window) != 0) {
            return -1;
        }
    }

    node = zereh_map_get(doc, filters, "qname_rules");
    if (node && node->type == YAML_MAPPING_NODE) {
        yaml_node_t *v;

        v = zereh_map_get(doc, node, "prefix_match");
        if (v && zereh_scalar(v)) {
            snprintf(route->prefix, sizeof(route->prefix), "%s", zereh_scalar(v));
        }

        v = zereh_map_get(doc, node, "suffix_match");
        if (v && zereh_scalar(v)) {
            snprintf(route->suffix, sizeof(route->suffix), "%s", zereh_scalar(v));
        }

        v = zereh_map_get(doc, node, "min_labels");
        if (v && zereh_parse_u8(v, &route->min_labels) != 0) {
            return -1;
        }
        if (v) {
            *has_min_labels = 1;
        }

        v = zereh_map_get(doc, node, "max_labels");
        if (v && zereh_parse_u8(v, &route->max_labels) != 0) {
            return -1;
        }
        if (v) {
            *has_max_labels = 1;
        }

        v = zereh_map_get(doc, node, "min_length");
        if (v && zereh_parse_u16(v, &route->qname_min_len) != 0) {
            return -1;
        }
        if (v) {
            *has_min_length = 1;
        }

        v = zereh_map_get(doc, node, "max_length");
        if (v && zereh_parse_u16(v, &route->qname_max_len) != 0) {
            return -1;
        }
        if (v) {
            *has_max_length = 1;
        }
    }

    node = zereh_map_get(doc, filters, "rate_limit");
    if (node && node->type == YAML_MAPPING_NODE) {
        yaml_node_t *v = zereh_map_get(doc, node, "qps");
        if (v && zereh_parse_u32(v, &route->rate_limit_qps) != 0) {
            return -1;
        }
    }

    node = zereh_map_get(doc, filters, "dns_types");
    if (node && zereh_parse_route_dns_types(doc, node, route) != 0) {
        return -1;
    }

    node = zereh_map_get(doc, filters, "bloom_filter");
    if (node && node->type == YAML_MAPPING_NODE) {
        yaml_node_t *v;

        v = zereh_map_get(doc, node, "enabled");
        if (v && zereh_parse_bool(v, &route->bloom_enabled) != 0) {
            return -1;
        }

        v = zereh_map_get(doc, node, "size");
        if (v && zereh_parse_u32(v, &route->bloom_size) != 0) {
            return -1;
        }

        v = zereh_map_get(doc, node, "hash_functions");
        if (v && zereh_parse_u8(v, &route->bloom_hash_functions) != 0) {
            return -1;
        }

        v = zereh_map_get(doc, node, "time_window");
        if (v && zereh_parse_u16(v, &route->bloom_time_window) != 0) {
            return -1;
        }

        v = zereh_map_get(doc, node, "allow_qnames");
        if (!v) {
            v = zereh_map_get(doc, node, "seed_qnames");
        }
        if (!v) {
            v = zereh_map_get(doc, node, "seeds");
        }
        if (v && zereh_parse_bloom_seeds(doc, v, route) != 0) {
            return -1;
        }
    }

    return 0;
}

static int zereh_parse_route(yaml_document_t *doc, yaml_node_t *node, const struct zereh_config *cfg, struct zereh_route *route)
{
    yaml_node_t *v;
    const char *domain;
    const char *target;
    int has_min_labels = 0;
    int has_max_labels = 0;
    int has_min_length = 0;
    int has_max_length = 0;

    if (!doc || !node || !cfg || !route || node->type != YAML_MAPPING_NODE) {
        return -1;
    }

    memset(route, 0, sizeof(*route));
    route->target = ZEREH_TARGET_USER_SPACE;
    route->kernel_port = cfg->default_kernel_port;
    route->xsk_queue = 0;
    route->prog_index = 0;
    route->min_labels = 0;
    route->max_labels = 0;
    route->qname_min_len = 0;
    route->qname_max_len = 0;
    route->rrtype_mask = ZEREH_RRTYPE_A | ZEREH_RRTYPE_AAAA | ZEREH_RRTYPE_TXT;
    route->pow_enabled = 1;
    route->pow_difficulty = 0;
    route->pow_time_window = 0;
    route->bloom_enabled = 0;
    route->bloom_size = 1024;
    route->bloom_hash_functions = 3;
    route->bloom_time_window = 0;
    route->rate_limit_qps = 0;

    v = zereh_map_get(doc, node, "domain");
    domain = zereh_scalar(v);
    if (!domain || domain[0] == '\0') {
        fprintf(stderr, "route missing domain\n");
        return -1;
    }
    snprintf(route->domain, sizeof(route->domain), "%s", domain);
    route->hash = zereh_hash_domain(route->domain, cfg->hash_algo);

    v = zereh_map_get(doc, node, "target_type");
    if (!v) {
        v = zereh_map_get(doc, node, "target");
    }
    target = zereh_scalar(v);
    if (!target) {
        fprintf(stderr, "route %s missing target_type/target\n", route->domain);
        return -1;
    }
    if (zereh_parse_target(target, &route->target) != 0) {
        fprintf(stderr, "route %s has unsupported target_type '%s'\n", route->domain, target);
        return -1;
    }

    v = zereh_map_get(doc, node, "xsk_map_index");
    if (!v) {
        v = zereh_map_get(doc, node, "xsk_queue");
    }
    if (v && zereh_parse_u32(v, &route->xsk_queue) != 0) {
        return -1;
    }

    v = zereh_map_get(doc, node, "forward_port");
    if (!v) {
        v = zereh_map_get(doc, node, "kernel_port");
    }
    if (v && zereh_parse_u16(v, &route->kernel_port) != 0) {
        return -1;
    }

    v = zereh_map_get(doc, node, "prog_array_index");
    if (!v) {
        v = zereh_map_get(doc, node, "prog_index");
    }
    if (v && zereh_parse_u32(v, &route->prog_index) != 0) {
        return -1;
    }

    v = zereh_map_get(doc, node, "prog_name");
    if (!v) {
        v = zereh_map_get(doc, node, "xdp_prog_name");
    }
    if (v && zereh_scalar(v)) {
        snprintf(route->prog_name, sizeof(route->prog_name), "%s", zereh_scalar(v));
    }

    v = zereh_map_get(doc, node, "prefix");
    if (v && zereh_scalar(v)) {
        snprintf(route->prefix, sizeof(route->prefix), "%s", zereh_scalar(v));
    }

    v = zereh_map_get(doc, node, "suffix");
    if (v && zereh_scalar(v)) {
        snprintf(route->suffix, sizeof(route->suffix), "%s", zereh_scalar(v));
    }

    v = zereh_map_get(doc, node, "min_labels");
    if (v && zereh_parse_u8(v, &route->min_labels) != 0) {
        return -1;
    }
    if (v) {
        has_min_labels = 1;
    }

    v = zereh_map_get(doc, node, "max_labels");
    if (v && zereh_parse_u8(v, &route->max_labels) != 0) {
        return -1;
    }
    if (v) {
        has_max_labels = 1;
    }

    v = zereh_map_get(doc, node, "qname_min_len");
    if (v && zereh_parse_u16(v, &route->qname_min_len) != 0) {
        return -1;
    }
    if (v) {
        has_min_length = 1;
    }

    if (!v) {
        v = zereh_map_get(doc, node, "min_length");
        if (v && zereh_parse_u16(v, &route->qname_min_len) != 0) {
            return -1;
        }
        if (v) {
            has_min_length = 1;
        }
    }

    v = zereh_map_get(doc, node, "qname_max_len");
    if (v && zereh_parse_u16(v, &route->qname_max_len) != 0) {
        return -1;
    }
    if (v) {
        has_max_length = 1;
    }

    if (!v) {
        v = zereh_map_get(doc, node, "max_length");
        if (v && zereh_parse_u16(v, &route->qname_max_len) != 0) {
            return -1;
        }
        if (v) {
            has_max_length = 1;
        }
    }

    v = zereh_map_get(doc, node, "pow_difficulty");
    if (v && zereh_parse_u8(v, &route->pow_difficulty) != 0) {
        return -1;
    }

    v = zereh_map_get(doc, node, "allow_types");
    if (v && zereh_parse_rrtype_mask(doc, v, &route->rrtype_mask) != 0) {
        return -1;
    }

    v = zereh_map_get(doc, node, "filters");
    if (v && zereh_parse_route_filters(doc,
                                       v,
                                       route,
                                       &has_min_labels,
                                       &has_max_labels,
                                       &has_min_length,
                                       &has_max_length) != 0) {
        return -1;
    }

    if (!has_min_labels && !has_max_labels) {
        route->min_labels = 1;
        route->max_labels = 127;
    } else if (has_min_labels && !has_max_labels) {
        route->max_labels = route->min_labels;
    } else if (!has_min_labels && has_max_labels) {
        route->min_labels = route->max_labels;
    }

    if (!has_min_length && !has_max_length) {
        route->qname_min_len = 1;
        route->qname_max_len = 255;
    } else if (has_min_length && !has_max_length) {
        route->qname_max_len = route->qname_min_len;
    } else if (!has_min_length && has_max_length) {
        route->qname_min_len = route->qname_max_len;
    }

    if (route->min_labels == 0 || route->max_labels == 0 ||
        route->min_labels > route->max_labels || route->max_labels > 127) {
        return -1;
    }

    if (route->qname_min_len == 0 || route->qname_max_len > 255 || route->qname_min_len > route->qname_max_len) {
        return -1;
    }

    if (!route->pow_enabled) {
        route->pow_difficulty = 0;
        route->pow_time_window = 0;
    }

    if (route->pow_difficulty > 63) {
        return -1;
    }

    if (!route->bloom_enabled) {
        route->bloom_size = 0;
        route->bloom_hash_functions = 0;
        route->bloom_time_window = 0;
        route->bloom_seed_count = 0;
    } else {
        if (route->bloom_size == 0) {
            route->bloom_size = 1024;
        }
        if (route->bloom_hash_functions == 0) {
            route->bloom_hash_functions = 3;
        }
        if (route->bloom_size > 65536 || route->bloom_size < 64) {
            return -1;
        }
        if (route->bloom_hash_functions > 8) {
            return -1;
        }

        if (route->bloom_seed_count == 0) {
            size_t domain_len = strnlen(route->domain, sizeof(route->bloom_seeds[0]) - 1);
            memmove(route->bloom_seeds[0], route->domain, domain_len);
            route->bloom_seeds[0][domain_len] = '\0';
            route->bloom_seed_count = 1;
        }
    }

    return 0;
}

static int zereh_parse_routes(yaml_document_t *doc, yaml_node_t *node, struct zereh_config *cfg)
{
    yaml_node_item_t *it;

    if (!node || !cfg || node->type != YAML_SEQUENCE_NODE) {
        return -1;
    }

    cfg->route_count = 0;

    for (it = node->data.sequence.items.start; it < node->data.sequence.items.top; it++) {
        yaml_node_t *entry = yaml_document_get_node(doc, *it);

        if (cfg->route_count >= ZEREH_MAX_ROUTES) {
            fprintf(stderr, "routes exceed max entries (%u)\n", ZEREH_MAX_ROUTES);
            return -1;
        }

        if (zereh_parse_route(doc, entry, cfg, &cfg->routes[cfg->route_count]) != 0) {
            return -1;
        }
        cfg->route_count++;
    }

    return 0;
}

static int zereh_parse_global_section(yaml_document_t *doc, yaml_node_t *root, struct zereh_config *cfg)
{
    yaml_node_t *global;
    yaml_node_t *v;

    global = zereh_map_get(doc, root, "global");
    if (global && global->type == YAML_MAPPING_NODE) {
        v = zereh_map_get(doc, global, "interface");
        if (v && zereh_scalar(v)) {
            snprintf(cfg->interface, sizeof(cfg->interface), "%s", zereh_scalar(v));
        }

        v = zereh_map_get(doc, global, "xdp_mode");
        if (v && zereh_parse_xdp_mode(v, &cfg->xdp_mode) != 0) {
            return -1;
        }

        v = zereh_map_get(doc, global, "default_action");
        if (v && zereh_parse_default_action(v, &cfg->default_action) != 0) {
            return -1;
        }
    }

    v = zereh_map_get(doc, root, "interface");
    if (v && zereh_scalar(v)) {
        snprintf(cfg->interface, sizeof(cfg->interface), "%s", zereh_scalar(v));
    }

    v = zereh_map_get(doc, root, "xdp_mode");
    if (v && zereh_parse_xdp_mode(v, &cfg->xdp_mode) != 0) {
        return -1;
    }

    v = zereh_map_get(doc, root, "default_action");
    if (v && zereh_parse_default_action(v, &cfg->default_action) != 0) {
        return -1;
    }

    return 0;
}

static int zereh_parse_global_filters(yaml_document_t *doc, yaml_node_t *root, struct zereh_config *cfg)
{
    yaml_node_t *filters;
    yaml_node_t *v;

    filters = zereh_map_get(doc, root, "global_filters");
    if (filters && filters->type == YAML_MAPPING_NODE) {
        v = zereh_map_get(doc, filters, "ip_blacklist");
        if (v && zereh_parse_blacklist(doc, v, cfg) != 0) {
            return -1;
        }

        v = zereh_map_get(doc, filters, "max_packet_size");
        if (v && zereh_parse_u32(v, &cfg->max_packet_size) != 0) {
            return -1;
        }

        v = zereh_map_get(doc, filters, "allowed_opcodes");
        if (v && zereh_parse_allowed_opcodes(doc, v, cfg) != 0) {
            return -1;
        }

        v = zereh_map_get(doc, filters, "drop_truncated");
        if (v && zereh_parse_bool(v, &cfg->drop_truncated) != 0) {
            return -1;
        }
    }

    v = zereh_map_get(doc, root, "blacklist");
    if (v && zereh_parse_blacklist(doc, v, cfg) != 0) {
        return -1;
    }

    v = zereh_map_get(doc, root, "max_packet_size");
    if (v && zereh_parse_u32(v, &cfg->max_packet_size) != 0) {
        return -1;
    }

    v = zereh_map_get(doc, root, "allowed_opcodes");
    if (v && zereh_parse_allowed_opcodes(doc, v, cfg) != 0) {
        return -1;
    }

    v = zereh_map_get(doc, root, "drop_truncated");
    if (v && zereh_parse_bool(v, &cfg->drop_truncated) != 0) {
        return -1;
    }

    return 0;
}

static int zereh_parse_codegen_options(yaml_document_t *doc, yaml_node_t *root, struct zereh_config *cfg)
{
    yaml_node_t *options;
    yaml_node_t *v;

    options = zereh_map_get(doc, root, "codegen_options");
    if (!options || options->type != YAML_MAPPING_NODE) {
        return 0;
    }

    v = zereh_map_get(doc, options, "hash_algorithm");
    if (v && zereh_parse_hash_algorithm(v, cfg) != 0) {
        return -1;
    }

    v = zereh_map_get(doc, options, "optimize_jump_tables");
    if (v && zereh_parse_bool(v, &cfg->optimize_jump_tables) != 0) {
        return -1;
    }

    v = zereh_map_get(doc, options, "inline_checksums");
    if (v && zereh_parse_bool(v, &cfg->inline_checksums) != 0) {
        return -1;
    }

    return 0;
}

int zereh_parse_config_yaml(const char *path, struct zereh_config *cfg)
{
    FILE *fp = NULL;
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_node_t *root;
    yaml_node_t *node;
    int ret = -1;

    if (!path || !cfg) {
        return -1;
    }

    zereh_config_init_defaults(cfg);

    fp = fopen(path, "r");
    if (!fp) {
        perror("fopen config");
        return -1;
    }

    if (!yaml_parser_initialize(&parser)) {
        fclose(fp);
        fprintf(stderr, "yaml_parser_initialize failed\n");
        return -1;
    }

    yaml_parser_set_input_file(&parser, fp);

    if (!yaml_parser_load(&parser, &doc)) {
        fprintf(stderr,
                "yaml_parser_load failed: line=%lu column=%lu\n",
                parser.problem_mark.line + 1,
                parser.problem_mark.column + 1);
        goto out_parser;
    }

    root = yaml_document_get_root_node(&doc);
    if (!root || root->type != YAML_MAPPING_NODE) {
        fprintf(stderr, "config root must be a mapping\n");
        goto out_doc;
    }

    if (zereh_parse_global_section(&doc, root, cfg) != 0) {
        fprintf(stderr, "invalid global section\n");
        goto out_doc;
    }

    if (zereh_parse_global_filters(&doc, root, cfg) != 0) {
        fprintf(stderr, "invalid global_filters section\n");
        goto out_doc;
    }

    node = zereh_map_get(&doc, root, "template_path");
    if (node && zereh_scalar(node)) {
        snprintf(cfg->template_path, sizeof(cfg->template_path), "%s", zereh_scalar(node));
    }

    node = zereh_map_get(&doc, root, "generated_source");
    if (node && zereh_scalar(node)) {
        snprintf(cfg->generated_source, sizeof(cfg->generated_source), "%s", zereh_scalar(node));
    }

    node = zereh_map_get(&doc, root, "generated_object");
    if (node && zereh_scalar(node)) {
        snprintf(cfg->generated_object, sizeof(cfg->generated_object), "%s", zereh_scalar(node));
    }

    node = zereh_map_get(&doc, root, "xdp_section");
    if (node && zereh_scalar(node)) {
        snprintf(cfg->xdp_section, sizeof(cfg->xdp_section), "%s", zereh_scalar(node));
    }

    node = zereh_map_get(&doc, root, "default_kernel_port");
    if (node && zereh_parse_u16(node, &cfg->default_kernel_port) != 0) {
        fprintf(stderr, "invalid default_kernel_port\n");
        goto out_doc;
    }

    if (zereh_parse_codegen_options(&doc, root, cfg) != 0) {
        fprintf(stderr, "invalid codegen_options\n");
        goto out_doc;
    }

    node = zereh_map_get(&doc, root, "routes");
    if (!node || zereh_parse_routes(&doc, node, cfg) != 0) {
        fprintf(stderr, "invalid routes\n");
        goto out_doc;
    }

    if (cfg->route_count == 0) {
        fprintf(stderr, "at least one route is required\n");
        goto out_doc;
    }

    ret = 0;

out_doc:
    yaml_document_delete(&doc);
out_parser:
    yaml_parser_delete(&parser);
    fclose(fp);
    return ret;
}

#endif /* ZEREH_HAVE_LIBYAML */
