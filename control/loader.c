#include "loader.h"

#include "hash.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#if __has_include(<bpf/bpf.h>) && __has_include(<bpf/libbpf.h>)
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <linux/if_link.h>
#define ZEREH_HAVE_LIBBPF 1
#else
#define ZEREH_HAVE_LIBBPF 0
#endif

#define ZEREH_BLOOM_WORDS 1024u
#define ZEREH_BLOOM_BITS (ZEREH_BLOOM_WORDS * 64u)
#define ZEREH_MAX_HASH_FUNCTIONS 8u
#define ZEREH_PROG_ARRAY_MAX 64u

static bool zereh_structured_logs_enabled(void)
{
    static int initialized;
    static bool enabled;

    if (!initialized) {
        const char *mode = getenv("ZEREH_LOG_MODE");
        enabled = mode && strcmp(mode, "structured") == 0;
        initialized = 1;
    }

    return enabled;
}

static void zereh_log_timestamp_utc(char *out, size_t out_len)
{
    time_t now;
    struct tm tmv;

    if (!out || out_len == 0) {
        return;
    }

    now = time(NULL);
    if (gmtime_r(&now, &tmv)) {
        strftime(out, out_len, "%Y-%m-%dT%H:%M:%SZ", &tmv);
        return;
    }

    snprintf(out, out_len, "unknown");
}

static void zereh_log_sanitize_message(const char *in, char *out, size_t out_len)
{
    size_t i = 0;
    size_t j = 0;

    if (!out || out_len == 0) {
        return;
    }

    if (!in) {
        out[0] = '\0';
        return;
    }

    while (in[i] != '\0' && (j + 1) < out_len) {
        char c = in[i++];
        if (c == '"') {
            c = '\'';
        } else if (c == '\n' || c == '\r' || c == '\t') {
            c = ' ';
        }
        out[j++] = c;
    }
    out[j] = '\0';
}

static void zereh_log_emit(FILE *stream, const char *level, const char *fmt, ...)
{
    char message[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);

    if (zereh_structured_logs_enabled()) {
        char ts[32];
        char sanitized[512];
        zereh_log_timestamp_utc(ts, sizeof(ts));
        zereh_log_sanitize_message(message, sanitized, sizeof(sanitized));
        fprintf(stream, "ts=%s level=%s component=loader msg=\"%s\"\n", ts, level, sanitized);
        return;
    }

    fprintf(stream, "[loader] [%s] %s\n", level, message);
}

#define ZEREH_LOG_INFO(...) zereh_log_emit(stdout, "info", __VA_ARGS__)
#define ZEREH_LOG_WARN(...) zereh_log_emit(stderr, "warn", __VA_ARGS__)
#define ZEREH_LOG_ERROR(...) zereh_log_emit(stderr, "error", __VA_ARGS__)

#if !ZEREH_HAVE_LIBBPF

int zereh_load_and_swap_xdp(const struct zereh_config *cfg, const char *ifname_override)
{
    (void)cfg;
    (void)ifname_override;
    ZEREH_LOG_ERROR("libbpf headers are not available; install libbpf development package");
    return -1;
}

int zereh_detach_xdp(const struct zereh_config *cfg, const char *ifname_override)
{
    (void)cfg;
    (void)ifname_override;
    ZEREH_LOG_ERROR("libbpf headers are not available; install libbpf development package");
    return -1;
}

#else

struct lpm_v4_key {
    uint32_t prefixlen;
    uint32_t addr;
};

static int zereh_raise_memlock(void)
{
    struct rlimit rlim = {
        .rlim_cur = RLIM_INFINITY,
        .rlim_max = RLIM_INFINITY,
    };

    if (setrlimit(RLIMIT_MEMLOCK, &rlim) != 0) {
        perror("setrlimit RLIMIT_MEMLOCK");
        return -1;
    }

    return 0;
}

static int zereh_ensure_dir(const char *path)
{
    struct stat st;

    if (stat(path, &st) == 0) {
        if (!S_ISDIR(st.st_mode)) {
            ZEREH_LOG_ERROR("%s exists but is not a directory", path);
            return -1;
        }
        return 0;
    }

    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        perror("mkdir");
        return -1;
    }

    return 0;
}

static int zereh_build_pin_path(char *out, size_t out_len, const char *ifname)
{
    static const char pin_dir[] = "/sys/fs/bpf/zereh";
    static const char link_prefix[] = "/xdp_link_";
    size_t ifname_len;
    size_t need;

    if (!out || out_len == 0 || !ifname || ifname[0] == '\0') {
        return -1;
    }

    ifname_len = strnlen(ifname, IFNAMSIZ);
    if (ifname_len == 0 || ifname_len >= IFNAMSIZ) {
        ZEREH_LOG_ERROR("interface name is invalid or too long: %s", ifname);
        return -1;
    }

    need = (sizeof(pin_dir) - 1) + (sizeof(link_prefix) - 1) + ifname_len + 1;
    if (need > out_len) {
        ZEREH_LOG_ERROR("pin path buffer too small for interface %s", ifname);
        return -1;
    }

    memcpy(out, pin_dir, sizeof(pin_dir) - 1);
    memcpy(out + (sizeof(pin_dir) - 1), link_prefix, sizeof(link_prefix) - 1);
    memmove(out + (sizeof(pin_dir) - 1) + (sizeof(link_prefix) - 1), ifname, ifname_len);
    out[need - 1] = '\0';

    return 0;
}

static int zereh_parse_cidr_v4(const char *cidr, struct lpm_v4_key *key)
{
    char tmp[ZEREH_MAX_CIDR_LEN];
    char *slash;
    struct in_addr addr;
    long prefix = 32;

    if (!cidr || !key) {
        return -1;
    }

    snprintf(tmp, sizeof(tmp), "%s", cidr);
    slash = strchr(tmp, '/');
    if (slash) {
        char *end = NULL;
        *slash = '\0';
        prefix = strtol(slash + 1, &end, 10);
        if (end == slash + 1 || *end != '\0' || prefix < 0 || prefix > 32) {
            return -1;
        }
    }

    if (inet_pton(AF_INET, tmp, &addr) != 1) {
        return -1;
    }

    key->prefixlen = (uint32_t)prefix;
    key->addr = addr.s_addr;
    return 0;
}

static int zereh_seed_blacklist_map(int map_fd, const struct zereh_config *cfg)
{
    uint32_t i;

    for (i = 0; i < cfg->blacklist_count; i++) {
        struct lpm_v4_key key;
        uint8_t deny = 1;

        if (zereh_parse_cidr_v4(cfg->blacklist[i].cidr, &key) != 0) {
            ZEREH_LOG_ERROR("invalid CIDR in blacklist: %s", cfg->blacklist[i].cidr);
            return -1;
        }

        if (bpf_map_update_elem(map_fd, &key, &deny, BPF_ANY) != 0) {
            perror("bpf_map_update_elem ip_blacklist");
            return -1;
        }
    }

    return 0;
}

static uint64_t zereh_mix64(uint64_t x)
{
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

static uint32_t zereh_route_bloom_bits(const struct zereh_route *route)
{
    uint32_t bits;

    if (!route->bloom_enabled) {
        return 0;
    }

    bits = route->bloom_size;
    if (bits < 64) {
        bits = 64;
    }
    if (bits > ZEREH_BLOOM_BITS) {
        bits = ZEREH_BLOOM_BITS;
    }

    return bits;
}

static uint8_t zereh_route_bloom_hash_functions(const struct zereh_route *route)
{
    uint8_t hf = route->bloom_hash_functions;

    if (!route->bloom_enabled) {
        return 0;
    }

    if (hf == 0) {
        hf = 3;
    }
    if (hf > ZEREH_MAX_HASH_FUNCTIONS) {
        hf = ZEREH_MAX_HASH_FUNCTIONS;
    }

    return hf;
}

static void zereh_bloom_set_bits_for_seed(uint64_t *words,
                                          uint64_t route_hash,
                                          uint32_t bloom_bits,
                                          uint8_t hash_functions,
                                          uint64_t bucket)
{
    uint8_t k;

    for (k = 0; k < hash_functions; k++) {
        uint64_t mixed;
        uint32_t bit;
        uint32_t idx;
        uint32_t off;

        mixed = zereh_mix64(route_hash ^ (((uint64_t)k + 1ULL) * 0x9e3779b97f4a7c15ULL) ^
                            (bucket * 0x517cc1b727220a95ULL));
        bit = (uint32_t)(mixed % bloom_bits);
        idx = bit >> 6;
        off = bit & 63;

        if (idx < ZEREH_BLOOM_WORDS) {
            words[idx] |= (1ULL << off);
        }
    }
}

static int zereh_seed_bloom_map(int map_fd, const struct zereh_config *cfg)
{
    uint64_t words[ZEREH_BLOOM_WORDS] = {0};
    uint32_t i;

    for (i = 0; i < cfg->route_count; i++) {
        const struct zereh_route *route = &cfg->routes[i];
        uint32_t bloom_bits;
        uint8_t hash_functions;
        uint64_t now_sec;
        uint64_t bucket = 0;
        uint8_t s;

        if (!route->bloom_enabled) {
            continue;
        }

        bloom_bits = zereh_route_bloom_bits(route);
        hash_functions = zereh_route_bloom_hash_functions(route);
        if (bloom_bits == 0 || hash_functions == 0) {
            continue;
        }

        if (route->bloom_time_window > 0) {
            now_sec = (uint64_t)time(NULL);
            bucket = now_sec / (uint64_t)route->bloom_time_window;
        }

        for (s = 0; s < route->bloom_seed_count; s++) {
            uint64_t seed_hash = zereh_hash_domain(route->bloom_seeds[s], cfg->hash_algo);

            if (seed_hash == 0) {
                continue;
            }

            zereh_bloom_set_bits_for_seed(words, seed_hash, bloom_bits, hash_functions, bucket);
            if (route->bloom_time_window > 0 && bucket > 0) {
                zereh_bloom_set_bits_for_seed(words,
                                              seed_hash,
                                              bloom_bits,
                                              hash_functions,
                                              bucket - 1);
            }
        }
    }

    for (i = 0; i < ZEREH_BLOOM_WORDS; i++) {
        uint32_t key = i;
        uint64_t value = words[i];

        if (bpf_map_update_elem(map_fd, &key, &value, BPF_ANY) != 0) {
            perror("bpf_map_update_elem pow_bloom");
            return -1;
        }
    }

    return 0;
}

static struct bpf_program *zereh_resolve_tail_program(struct bpf_object *obj,
                                                       const struct zereh_route *route,
                                                       struct bpf_program *default_tail,
                                                       char *resolved_name,
                                                       size_t resolved_name_len)
{
    struct bpf_program *tail_prog;
    char indexed_name[ZEREH_MAX_PROG_NAME];

    if (!obj || !route || !resolved_name || resolved_name_len == 0) {
        return NULL;
    }

    if (route->prog_name[0] != '\0') {
        tail_prog = bpf_object__find_program_by_name(obj, route->prog_name);
        if (!tail_prog) {
            ZEREH_LOG_ERROR("xdp_prog route '%s' requested prog_name '%s' but it was not found",
                            route->domain,
                            route->prog_name);
            return NULL;
        }
        snprintf(resolved_name, resolved_name_len, "%s", route->prog_name);
        return tail_prog;
    }

    snprintf(indexed_name, sizeof(indexed_name), "zereh_app_%u", route->prog_index);
    tail_prog = bpf_object__find_program_by_name(obj, indexed_name);
    if (tail_prog) {
        snprintf(resolved_name, resolved_name_len, "%s", indexed_name);
        return tail_prog;
    }

    if (default_tail) {
        snprintf(resolved_name, resolved_name_len, "%s", "zereh_app_default");
        return default_tail;
    }

    ZEREH_LOG_ERROR("xdp_prog route '%s' has no tail program (expected prog_name, %s, or zereh_app_default)",
                    route->domain,
                    indexed_name);
    return NULL;
}

static int zereh_seed_prog_array(int map_fd, struct bpf_object *obj, const struct zereh_config *cfg)
{
    struct bpf_program *default_tail;
    uint32_t i;

    default_tail = bpf_object__find_program_by_name(obj, "zereh_app_default");

    for (i = 0; i < cfg->route_count; i++) {
        const struct zereh_route *route;
        struct bpf_program *tail_prog;
        char resolved_name[ZEREH_MAX_PROG_NAME];
        uint32_t key;
        int value;

        if (cfg->routes[i].target != ZEREH_TARGET_XDP_PROG) {
            continue;
        }

        route = &cfg->routes[i];
        key = route->prog_index;
        if (key >= ZEREH_PROG_ARRAY_MAX) {
            ZEREH_LOG_ERROR("route '%s' prog_array_index %u exceeds prog_array max %u",
                            route->domain,
                            key,
                            ZEREH_PROG_ARRAY_MAX);
            return -1;
        }

        tail_prog = zereh_resolve_tail_program(obj,
                                               route,
                                               default_tail,
                                               resolved_name,
                                               sizeof(resolved_name));
        if (!tail_prog) {
            return -1;
        }

        value = bpf_program__fd(tail_prog);
        if (value < 0) {
            ZEREH_LOG_ERROR("failed to get fd for tail program '%s'", resolved_name);
            return -1;
        }

        if (bpf_map_update_elem(map_fd, &key, &value, BPF_ANY) != 0) {
            perror("bpf_map_update_elem prog_array");
            return -1;
        }
    }

    return 0;
}

static __u32 zereh_xdp_mode_to_flags(enum zereh_xdp_mode mode)
{
    switch (mode) {
    case ZEREH_XDP_MODE_SKB:
        return XDP_FLAGS_SKB_MODE;
    case ZEREH_XDP_MODE_OFFLOAD:
        return XDP_FLAGS_HW_MODE;
    case ZEREH_XDP_MODE_NATIVE:
    default:
        return XDP_FLAGS_DRV_MODE;
    }
}

static const char *zereh_xdp_mode_name(enum zereh_xdp_mode mode)
{
    switch (mode) {
    case ZEREH_XDP_MODE_SKB:
        return "skb";
    case ZEREH_XDP_MODE_OFFLOAD:
        return "offload";
    case ZEREH_XDP_MODE_NATIVE:
    default:
        return "native";
    }
}

static int zereh_attach_xdp_replace(int ifindex, int prog_fd, enum zereh_xdp_mode mode)
{
    __u32 flags = zereh_xdp_mode_to_flags(mode);
    __u32 old_prog_id = 0;
    struct bpf_xdp_attach_opts opts;
    int old_prog_fd = -1;

    memset(&opts, 0, sizeof(opts));
    opts.sz = sizeof(opts);

    if (bpf_xdp_query_id(ifindex, flags, &old_prog_id) == 0 && old_prog_id > 0) {
        old_prog_fd = bpf_prog_get_fd_by_id(old_prog_id);
        if (old_prog_fd >= 0) {
            opts.old_prog_fd = old_prog_fd;
        }
    }

    if (bpf_xdp_attach(ifindex, prog_fd, flags, &opts) != 0) {
        perror("bpf_xdp_attach");
        if (old_prog_fd >= 0) {
            close(old_prog_fd);
        }
        return -1;
    }

    if (old_prog_fd >= 0) {
        close(old_prog_fd);
    }

    return 0;
}

static int zereh_attach_or_update_link(struct bpf_program *prog,
                                       int ifindex,
                                       const char *pin_path,
                                       enum zereh_xdp_mode mode)
{
    int old_link_fd;
    struct bpf_link *link;

    if (mode != ZEREH_XDP_MODE_NATIVE) {
        if (zereh_attach_xdp_replace(ifindex, bpf_program__fd(prog), mode) != 0) {
            return -1;
        }
        return 0;
    }

    old_link_fd = bpf_obj_get(pin_path);
    if (old_link_fd >= 0) {
        if (bpf_link_update(old_link_fd, bpf_program__fd(prog), NULL) != 0) {
            perror("bpf_link_update");
            close(old_link_fd);
            return -1;
        }
        close(old_link_fd);
        return 0;
    }

    if (errno != ENOENT) {
        perror("bpf_obj_get link");
        return -1;
    }

    link = bpf_program__attach_xdp(prog, ifindex);
    if (libbpf_get_error(link)) {
        ZEREH_LOG_ERROR("bpf_program__attach_xdp failed");
        return -1;
    }

    if (bpf_link__pin(link, pin_path) != 0) {
        perror("bpf_link__pin");
        bpf_link__destroy(link);
        return -1;
    }

    bpf_link__destroy(link);
    return 0;
}

static int zereh_attach_with_fallback(struct bpf_program *prog,
                                      int ifindex,
                                      const char *pin_path,
                                      enum zereh_xdp_mode requested_mode,
                                      enum zereh_xdp_mode *effective_mode)
{
    if (zereh_attach_or_update_link(prog, ifindex, pin_path, requested_mode) == 0) {
        *effective_mode = requested_mode;
        return 0;
    }

    if (requested_mode == ZEREH_XDP_MODE_OFFLOAD) {
        ZEREH_LOG_WARN("offload attach failed; falling back to native mode");
        if (zereh_attach_or_update_link(prog, ifindex, pin_path, ZEREH_XDP_MODE_NATIVE) == 0) {
            *effective_mode = ZEREH_XDP_MODE_NATIVE;
            return 0;
        }

        ZEREH_LOG_WARN("native attach failed; falling back to skb mode");
        if (zereh_attach_or_update_link(prog, ifindex, pin_path, ZEREH_XDP_MODE_SKB) == 0) {
            *effective_mode = ZEREH_XDP_MODE_SKB;
            return 0;
        }
    } else if (requested_mode == ZEREH_XDP_MODE_NATIVE) {
        ZEREH_LOG_WARN("native attach failed; falling back to skb mode");
        if (zereh_attach_or_update_link(prog, ifindex, pin_path, ZEREH_XDP_MODE_SKB) == 0) {
            *effective_mode = ZEREH_XDP_MODE_SKB;
            return 0;
        }
    }

    return -1;
}

static int zereh_detach_mode(int ifindex, __u32 flags, const char *mode_name, bool *detached_any)
{
    if (bpf_xdp_detach(ifindex, flags, NULL) == 0) {
        *detached_any = true;
        ZEREH_LOG_INFO("detached XDP program in %s mode", mode_name);
        return 0;
    }

    if (errno == ENOENT || errno == EINVAL || errno == ENOTSUP || errno == EOPNOTSUPP) {
        return 0;
    }

    perror("bpf_xdp_detach");
    return -1;
}

int zereh_load_and_swap_xdp(const struct zereh_config *cfg, const char *ifname_override)
{
    struct bpf_object *obj = NULL;
    struct bpf_program *prog = NULL;
    struct bpf_map *blacklist_map;
    struct bpf_map *prog_array_map;
    struct bpf_map *bloom_map;
    const char *ifname;
    enum zereh_xdp_mode effective_mode;
    int ifindex;
    char pin_path[PATH_MAX];
    int rc = -1;

    if (!cfg) {
        return -1;
    }

    ifname = (ifname_override && ifname_override[0] != '\0') ? ifname_override : cfg->interface;
    ifindex = if_nametoindex(ifname);
    if (ifindex == 0) {
        ZEREH_LOG_ERROR("if_nametoindex failed for interface %s", ifname);
        return -1;
    }

    if (zereh_raise_memlock() != 0) {
        return -1;
    }

    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

    obj = bpf_object__open_file(cfg->generated_object, NULL);
    if (libbpf_get_error(obj)) {
        ZEREH_LOG_ERROR("bpf_object__open_file failed for %s", cfg->generated_object);
        obj = NULL;
        return -1;
    }

    if (bpf_object__load(obj) != 0) {
        perror("bpf_object__load");
        goto out;
    }

    blacklist_map = bpf_object__find_map_by_name(obj, "ip_blacklist");
    if (blacklist_map && zereh_seed_blacklist_map(bpf_map__fd(blacklist_map), cfg) != 0) {
        goto out;
    }

    bloom_map = bpf_object__find_map_by_name(obj, "pow_bloom");
    if (bloom_map && zereh_seed_bloom_map(bpf_map__fd(bloom_map), cfg) != 0) {
        goto out;
    }

    prog_array_map = bpf_object__find_map_by_name(obj, "prog_array");
    if (prog_array_map && zereh_seed_prog_array(bpf_map__fd(prog_array_map), obj, cfg) != 0) {
        goto out;
    }

    prog = bpf_object__find_program_by_name(obj, cfg->xdp_section);
    if (!prog) {
        ZEREH_LOG_ERROR("program '%s' not found in object", cfg->xdp_section);
        goto out;
    }

    if (zereh_ensure_dir("/sys/fs/bpf/zereh") != 0) {
        goto out;
    }

    if (zereh_build_pin_path(pin_path, sizeof(pin_path), ifname) != 0) {
        goto out;
    }

    if (zereh_attach_with_fallback(prog, ifindex, pin_path, cfg->xdp_mode, &effective_mode) != 0) {
        goto out;
    }

    ZEREH_LOG_INFO("XDP program '%s' loaded on %s (ifindex=%d)", cfg->xdp_section, ifname, ifindex);
    ZEREH_LOG_INFO("XDP mode requested=%s effective=%s",
                   zereh_xdp_mode_name(cfg->xdp_mode),
                   zereh_xdp_mode_name(effective_mode));
    if (effective_mode == ZEREH_XDP_MODE_NATIVE) {
        ZEREH_LOG_INFO("pinned link: %s", pin_path);
    }

    rc = 0;
out:
    if (obj) {
        bpf_object__close(obj);
    }
    return rc;
}

int zereh_detach_xdp(const struct zereh_config *cfg, const char *ifname_override)
{
    const char *ifname;
    int ifindex;
    char pin_path[PATH_MAX];
    int link_fd;
    bool detached_any = false;

    if (!cfg) {
        return -1;
    }

    ifname = (ifname_override && ifname_override[0] != '\0') ? ifname_override : cfg->interface;
    ifindex = if_nametoindex(ifname);
    if (ifindex == 0) {
        ZEREH_LOG_ERROR("if_nametoindex failed for interface %s", ifname);
        return -1;
    }

    if (zereh_build_pin_path(pin_path, sizeof(pin_path), ifname) != 0) {
        return -1;
    }

    link_fd = bpf_obj_get(pin_path);
    if (link_fd >= 0) {
        close(link_fd);
        if (unlink(pin_path) != 0 && errno != ENOENT) {
            perror("unlink pinned xdp link");
            return -1;
        }
        ZEREH_LOG_INFO("removed pinned link: %s", pin_path);
    }

    if (zereh_detach_mode(ifindex, XDP_FLAGS_DRV_MODE, "native", &detached_any) != 0) {
        return -1;
    }
    if (zereh_detach_mode(ifindex, XDP_FLAGS_HW_MODE, "offload", &detached_any) != 0) {
        return -1;
    }
    if (zereh_detach_mode(ifindex, XDP_FLAGS_SKB_MODE, "skb", &detached_any) != 0) {
        return -1;
    }

    if (!detached_any) {
        ZEREH_LOG_INFO("no XDP program was attached on %s", ifname);
        return 0;
    }

    ZEREH_LOG_INFO("XDP detached on %s (ifindex=%d)", ifname, ifindex);
    return 0;
}

#endif /* ZEREH_HAVE_LIBBPF */
