#ifndef ZEREH_CONFIG_H
#define ZEREH_CONFIG_H

#include <limits.h>
#include <net/if.h>
#include <stdint.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define ZEREH_MAX_ROUTES 1024
#define ZEREH_MAX_BLACKLIST 1024
#define ZEREH_MAX_DOMAIN_LEN 255
#define ZEREH_MAX_PATTERN_LEN 128
#define ZEREH_MAX_CIDR_LEN 64
#define ZEREH_MAX_HASH_ALGO_LEN 32
#define ZEREH_MAX_BLOOM_SEEDS 32
#define ZEREH_MAX_PROG_NAME 64

enum zereh_target_type {
    ZEREH_TARGET_USER_SPACE = 0,
    ZEREH_TARGET_KERNEL_SPACE = 1,
    ZEREH_TARGET_XDP_PROG = 2,
};

enum zereh_default_action {
    ZEREH_ACTION_PASS = 0,
    ZEREH_ACTION_DROP = 1,
};

enum zereh_xdp_mode {
    ZEREH_XDP_MODE_NATIVE = 0,
    ZEREH_XDP_MODE_SKB = 1,
    ZEREH_XDP_MODE_OFFLOAD = 2,
};

enum zereh_hash_algorithm {
    ZEREH_HASH_FNV1A = 0,
    ZEREH_HASH_SIPHASH = 1,
};

enum zereh_rrtype_mask {
    ZEREH_RRTYPE_A = 1u << 0,
    ZEREH_RRTYPE_AAAA = 1u << 1,
    ZEREH_RRTYPE_TXT = 1u << 2,
    ZEREH_RRTYPE_ANY = 1u << 3,
};

struct zereh_blacklist_entry {
    char cidr[ZEREH_MAX_CIDR_LEN];
};

struct zereh_route {
    char domain[ZEREH_MAX_DOMAIN_LEN + 1];
    uint64_t hash;
    enum zereh_target_type target;

    uint32_t xsk_queue;
    uint16_t kernel_port;
    uint32_t prog_index;
    char prog_name[ZEREH_MAX_PROG_NAME];

    char prefix[ZEREH_MAX_PATTERN_LEN + 1];
    char suffix[ZEREH_MAX_PATTERN_LEN + 1];
    uint8_t min_labels;
    uint8_t max_labels;
    uint16_t qname_min_len;
    uint16_t qname_max_len;
    uint32_t rrtype_mask;
    uint8_t pow_enabled;
    uint8_t pow_difficulty;
    uint16_t pow_time_window;
    uint8_t bloom_enabled;
    uint32_t bloom_size;
    uint8_t bloom_hash_functions;
    uint16_t bloom_time_window;
    char bloom_seeds[ZEREH_MAX_BLOOM_SEEDS][ZEREH_MAX_DOMAIN_LEN + 1];
    uint8_t bloom_seed_count;
    uint32_t rate_limit_qps;
};

struct zereh_config {
    char interface[IFNAMSIZ];
    char template_path[PATH_MAX];
    char generated_source[PATH_MAX];
    char generated_object[PATH_MAX];
    char xdp_section[64];
    enum zereh_xdp_mode xdp_mode;
    enum zereh_default_action default_action;

    uint32_t max_packet_size;
    uint16_t default_kernel_port;
    uint16_t allowed_opcode_mask;
    uint8_t drop_truncated;

    char hash_algorithm[ZEREH_MAX_HASH_ALGO_LEN];
    enum zereh_hash_algorithm hash_algo;
    uint8_t optimize_jump_tables;
    uint8_t inline_checksums;

    struct zereh_blacklist_entry blacklist[ZEREH_MAX_BLACKLIST];
    uint32_t blacklist_count;

    struct zereh_route routes[ZEREH_MAX_ROUTES];
    uint32_t route_count;
};

void zereh_config_init_defaults(struct zereh_config *cfg);

#endif /* ZEREH_CONFIG_H */
