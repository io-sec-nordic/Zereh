/*
 * Zereh - eBPF/XDP Zero-Latency DNS Router & Edge Filter
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Commercial licensing available from IO-SEC Nordic AB.
 */

#include "dns_parser.h"

#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>

#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>

char LICENSE[] SEC("license") = "Dual AGPL-3.0-or-later OR Proprietary";

#define ZEREH_MAX_PACKET_SIZE __ZEREH_MAX_PACKET_SIZE__
#define ZEREH_ALLOWED_OPCODE_MASK (__ZEREH_ALLOWED_OPCODE_MASK__u)
#define ZEREH_DROP_TRUNCATED (__ZEREH_DROP_TRUNCATED__u)
#define ZEREH_DEFAULT_ACTION __ZEREH_DEFAULT_ACTION__
#define ZEREH_INLINE_CHECKSUMS (__ZEREH_INLINE_CHECKSUMS__u)
#define ZEREH_HASH_ALGO (__ZEREH_HASH_ALGO__u)
#define ZEREH_RATE_STATE_ENTRIES __ZEREH_RATE_STATE_ENTRIES__

#define ZEREH_HASH_FNV1A 0u
#define ZEREH_HASH_SIPHASH 1u

#define ZEREH_RRTYPE_A 0x1u
#define ZEREH_RRTYPE_AAAA 0x2u
#define ZEREH_RRTYPE_TXT 0x4u
#define ZEREH_RRTYPE_ANY 0x8u

#define ZEREH_BLOOM_WORDS 1024u
#define ZEREH_BLOOM_BITS (ZEREH_BLOOM_WORDS * 64u)
#define ZEREH_MAX_HASH_FUNCTIONS 8u

#define ZEREH_SIPHASH_K0 0x0706050403020100ULL
#define ZEREH_SIPHASH_K1 0x0f0e0d0c0b0a0908ULL

#if ZEREH_INLINE_CHECKSUMS
#define ZEREH_CSUM_INLINE __always_inline
#else
#define ZEREH_CSUM_INLINE
#endif

struct lpm_v4_key {
    __u32 prefixlen;
    __u32 addr;
};

struct rate_state {
    __u64 window_sec;
    __u32 count;
    __u32 _pad;
};

struct {
    __uint(type, BPF_MAP_TYPE_LPM_TRIE);
    __uint(max_entries, 1024);
    __uint(map_flags, BPF_F_NO_PREALLOC);
    __type(key, struct lpm_v4_key);
    __type(value, __u8);
} ip_blacklist SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_XSKMAP);
    __uint(max_entries, 64);
    __type(key, __u32);
    __type(value, __u32);
} xsk_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PROG_ARRAY);
    __uint(max_entries, 64);
    __type(key, __u32);
    __type(value, __u32);
} prog_array SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, ZEREH_BLOOM_WORDS);
    __type(key, __u32);
    __type(value, __u64);
} pow_bloom SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __uint(max_entries, ZEREH_RATE_STATE_ENTRIES);
    __type(key, __u32);
    __type(value, struct rate_state);
} route_rate_state SEC(".maps");

static __always_inline __u64 zereh_rotl64(__u64 x, __u32 b)
{
    return (x << b) | (x >> (64U - b));
}

static __always_inline void zereh_sipround(__u64 *v0, __u64 *v1, __u64 *v2, __u64 *v3)
{
    *v0 += *v1;
    *v1 = zereh_rotl64(*v1, 13);
    *v1 ^= *v0;
    *v0 = zereh_rotl64(*v0, 32);

    *v2 += *v3;
    *v3 = zereh_rotl64(*v3, 16);
    *v3 ^= *v2;

    *v0 += *v3;
    *v3 = zereh_rotl64(*v3, 21);
    *v3 ^= *v0;

    *v2 += *v1;
    *v1 = zereh_rotl64(*v1, 17);
    *v1 ^= *v2;
    *v2 = zereh_rotl64(*v2, 32);
}

static __always_inline __u64 zereh_siphash24_qname(const char *qname, __u16 qname_len)
{
    __u64 v0 = 0x736f6d6570736575ULL ^ ZEREH_SIPHASH_K0;
    __u64 v1 = 0x646f72616e646f6dULL ^ ZEREH_SIPHASH_K1;
    __u64 v2 = 0x6c7967656e657261ULL ^ ZEREH_SIPHASH_K0;
    __u64 v3 = 0x7465646279746573ULL ^ ZEREH_SIPHASH_K1;
    __u64 b = ((__u64)qname_len) << 56;
    __u32 blk;
    __u32 off = 0;

#pragma clang loop unroll(disable)
    for (blk = 0; blk < 31; blk++) {
        __u64 m = 0;
        __u32 j;

        off = blk * 8;
        if (off + 8 > qname_len) {
            break;
        }

#pragma clang loop unroll(full)
        for (j = 0; j < 8; j++) {
            m |= ((__u64)(__u8)qname[off + j]) << (8 * j);
        }

        v3 ^= m;
        zereh_sipround(&v0, &v1, &v2, &v3);
        zereh_sipround(&v0, &v1, &v2, &v3);
        v0 ^= m;
    }

    off = (qname_len / 8) * 8;

#pragma clang loop unroll(full)
    for (__u32 i = 0; i < 7; i++) {
        if (off + i >= qname_len) {
            break;
        }
        b |= ((__u64)(__u8)qname[off + i]) << (8 * i);
    }

    v3 ^= b;
    zereh_sipround(&v0, &v1, &v2, &v3);
    zereh_sipround(&v0, &v1, &v2, &v3);
    v0 ^= b;

    v2 ^= 0xff;
    zereh_sipround(&v0, &v1, &v2, &v3);
    zereh_sipround(&v0, &v1, &v2, &v3);
    zereh_sipround(&v0, &v1, &v2, &v3);
    zereh_sipround(&v0, &v1, &v2, &v3);

    return v0 ^ v1 ^ v2 ^ v3;
}

static __always_inline __u64 zereh_route_hash(const struct dns_query_meta *qmeta)
{
    if (ZEREH_HASH_ALGO == ZEREH_HASH_SIPHASH) {
        return zereh_siphash24_qname(qmeta->qname, qmeta->qname_len);
    }
    return qmeta->qname_hash;
}

static ZEREH_CSUM_INLINE void zereh_csum_replace_u16(__sum16 *check, __be16 oldv, __be16 newv)
{
    __u32 csum;

    csum = ~((__u16)*check) & 0xffff;
    csum += (~(__u16)oldv) & 0xffff;
    csum += (__u16)newv;
    csum = (csum & 0xffff) + (csum >> 16);
    csum = (csum & 0xffff) + (csum >> 16);
    *check = (__sum16)(~csum);
}

static ZEREH_CSUM_INLINE void zereh_csum_replace_u32(__sum16 *check, __be32 oldv, __be32 newv)
{
    zereh_csum_replace_u16(check, (__be16)(oldv >> 16), (__be16)(newv >> 16));
    zereh_csum_replace_u16(check, (__be16)(oldv & 0xffff), (__be16)(newv & 0xffff));
}

static __always_inline bool zereh_type_allowed(__u16 qtype, __u32 mask)
{
    if (qtype == 1 && (mask & ZEREH_RRTYPE_A)) {
        return true;
    }
    if (qtype == 28 && (mask & ZEREH_RRTYPE_AAAA)) {
        return true;
    }
    if (qtype == 16 && (mask & ZEREH_RRTYPE_TXT)) {
        return true;
    }
    if (qtype == 255 && (mask & ZEREH_RRTYPE_ANY)) {
        return true;
    }
    return false;
}

static __always_inline bool zereh_match_prefix(const char *qname,
                                                __u16 qname_len,
                                                const char *prefix,
                                                __u16 prefix_len)
{
    __u16 i;

    if (prefix_len == 0) {
        return true;
    }
    if (prefix_len > qname_len) {
        return false;
    }

#pragma clang loop unroll(disable)
    for (i = 0; i < 128; i++) {
        if (i >= prefix_len) {
            return true;
        }
        if (qname[i] != prefix[i]) {
            return false;
        }
    }

    return false;
}

static __always_inline bool zereh_match_suffix(const char *qname,
                                                __u16 qname_len,
                                                const char *suffix,
                                                __u16 suffix_len)
{
    __u16 start;
    __u16 i;

    if (suffix_len == 0) {
        return true;
    }
    if (suffix_len > qname_len) {
        return false;
    }

    start = qname_len - suffix_len;

#pragma clang loop unroll(disable)
    for (i = 0; i < 128; i++) {
        if (i >= suffix_len) {
            return true;
        }
        if (qname[start + i] != suffix[i]) {
            return false;
        }
    }

    return false;
}

static __always_inline __u64 zereh_mix64(__u64 x)
{
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

static __always_inline bool zereh_bloom_test_bit(__u32 bit, __u32 bloom_bits)
{
    __u32 idx = bit >> 6;
    __u32 off = bit & 63;
    __u64 *word;

    if (bloom_bits == 0 || bloom_bits > ZEREH_BLOOM_BITS || bit >= bloom_bits) {
        return false;
    }

    if (idx >= ZEREH_BLOOM_WORDS) {
        return false;
    }

    word = bpf_map_lookup_elem(&pow_bloom, &idx);
    if (!word) {
        return false;
    }

    return (((*word) >> off) & 1ULL) != 0;
}

static __always_inline bool zereh_bloom_bucket_match(__u64 qname_hash,
                                                      __u32 bloom_bits,
                                                      __u8 hash_functions,
                                                      __u64 bucket)
{
    __u8 k;

#pragma clang loop unroll(disable)
    for (k = 0; k < ZEREH_MAX_HASH_FUNCTIONS; k++) {
        __u64 mixed;
        __u32 bit;

        if (k >= hash_functions) {
            break;
        }

        mixed = zereh_mix64(qname_hash ^ (((__u64)k + 1ULL) * 0x9e3779b97f4a7c15ULL) ^ (bucket * 0x517cc1b727220a95ULL));
        bit = (__u32)(mixed % bloom_bits);

        if (!zereh_bloom_test_bit(bit, bloom_bits)) {
            return false;
        }
    }

    return true;
}

static __always_inline bool zereh_bloom_allow(__u64 qname_hash,
                                               __u32 bloom_bits,
                                               __u8 hash_functions,
                                               __u16 time_window)
{
    __u64 now_sec;
    __u64 bucket;

    if (hash_functions == 0 || bloom_bits < 64 || bloom_bits > ZEREH_BLOOM_BITS) {
        return false;
    }

    if (time_window == 0) {
        return zereh_bloom_bucket_match(qname_hash, bloom_bits, hash_functions, 0);
    }

    now_sec = bpf_ktime_get_ns() / 1000000000ULL;
    bucket = now_sec / (__u64)time_window;

    if (zereh_bloom_bucket_match(qname_hash, bloom_bits, hash_functions, bucket)) {
        return true;
    }

    if (bucket > 0 && zereh_bloom_bucket_match(qname_hash, bloom_bits, hash_functions, bucket - 1)) {
        return true;
    }

    return false;
}

static __always_inline bool zereh_pow_bucket_ok(__u64 qname_hash,
                                                 __u16 dns_id,
                                                 __u8 difficulty,
                                                 __u64 bucket)
{
    __u64 mixed;
    __u8 leading;

    mixed = qname_hash ^ (((__u64)dns_id) * 0x9e3779b97f4a7c15ULL) ^ (bucket * 0x517cc1b727220a95ULL);
    mixed = zereh_mix64(mixed);

    if (mixed == 0) {
        leading = 64;
    } else {
        leading = (__u8)__builtin_clzll(mixed);
    }

    return leading >= difficulty;
}

static __always_inline bool zereh_pow_allow(__u64 qname_hash,
                                             __u16 dns_id,
                                             __u8 difficulty,
                                             __u16 time_window)
{
    __u64 now_sec;
    __u64 bucket;

    if (difficulty == 0) {
        return true;
    }

    if (time_window == 0) {
        return zereh_pow_bucket_ok(qname_hash, dns_id, difficulty, 0);
    }

    now_sec = bpf_ktime_get_ns() / 1000000000ULL;
    bucket = now_sec / (__u64)time_window;

    if (zereh_pow_bucket_ok(qname_hash, dns_id, difficulty, bucket)) {
        return true;
    }

    if (bucket > 0 && zereh_pow_bucket_ok(qname_hash, dns_id, difficulty, bucket - 1)) {
        return true;
    }

    return false;
}

static __always_inline bool zereh_rate_limit_allow(__u32 route_id, __u32 qps)
{
    struct rate_state *state;
    __u64 now_sec;

    if (qps == 0) {
        return true;
    }

    if (route_id >= ZEREH_RATE_STATE_ENTRIES) {
        return false;
    }

    state = bpf_map_lookup_elem(&route_rate_state, &route_id);
    if (!state) {
        return false;
    }

    now_sec = bpf_ktime_get_ns() / 1000000000ULL;

    if (state->window_sec != now_sec) {
        state->window_sec = now_sec;
        state->count = 1;
        return true;
    }

    if (state->count >= qps) {
        return false;
    }

    state->count++;
    return true;
}

static __always_inline bool zereh_apply_app_filters(const struct dns_query_meta *qmeta,
                                                     const char *prefix,
                                                     __u16 prefix_len,
                                                     const char *suffix,
                                                     __u16 suffix_len,
                                                     __u8 min_labels,
                                                     __u8 max_labels,
                                                     __u16 qname_min_len,
                                                     __u16 qname_max_len,
                                                     __u64 route_hash,
                                                     __u32 route_id,
                                                     __u32 rrtype_mask,
                                                     __u8 pow_difficulty,
                                                     __u16 pow_time_window,
                                                     __u8 bloom_required,
                                                     __u32 bloom_size,
                                                     __u8 bloom_hash_functions,
                                                     __u16 bloom_time_window,
                                                     __u32 rate_limit_qps)
{
    if (qmeta->label_depth < min_labels || qmeta->label_depth > max_labels) {
        return false;
    }

    if (qmeta->qname_len < qname_min_len || qmeta->qname_len > qname_max_len) {
        return false;
    }

    if (!zereh_type_allowed(qmeta->qtype, rrtype_mask)) {
        return false;
    }

    if (!zereh_match_prefix(qmeta->qname, qmeta->qname_len, prefix, prefix_len)) {
        return false;
    }

    if (!zereh_match_suffix(qmeta->qname, qmeta->qname_len, suffix, suffix_len)) {
        return false;
    }

    if (!zereh_rate_limit_allow(route_id, rate_limit_qps)) {
        return false;
    }

    if (!zereh_pow_allow(route_hash, qmeta->dns_id, pow_difficulty, pow_time_window)) {
        return false;
    }

    if (bloom_required && !zereh_bloom_allow(route_hash,
                                             bloom_size,
                                             bloom_hash_functions,
                                             bloom_time_window)) {
        return false;
    }

    return true;
}

static __always_inline int zereh_global_pre_route(const struct iphdr *iph,
                                                   const struct dns_query_meta *qmeta,
                                                   __u32 packet_len)
{
    struct lpm_v4_key key = {
        .prefixlen = 32,
        .addr = iph->saddr,
    };
    __u8 *deny;

    if (packet_len > ZEREH_MAX_PACKET_SIZE) {
        return -1;
    }

    deny = bpf_map_lookup_elem(&ip_blacklist, &key);
    if (deny) {
        return -1;
    }

    if (qmeta->opcode > 15) {
        return -1;
    }

    if ((ZEREH_ALLOWED_OPCODE_MASK & (1u << qmeta->opcode)) == 0) {
        return -1;
    }

    if (ZEREH_DROP_TRUNCATED && qmeta->is_truncated) {
        return -1;
    }

    return 0;
}

static __always_inline int zereh_route_kernel_space(struct iphdr *iph,
                                                     struct udphdr *udp,
                                                     __be16 new_dport,
                                                     __be32 new_daddr)
{
    if (new_daddr != 0 && iph->daddr != new_daddr) {
        zereh_csum_replace_u32(&iph->check, iph->daddr, new_daddr);
        if (udp->check) {
            zereh_csum_replace_u32(&udp->check, iph->daddr, new_daddr);
        }
        iph->daddr = new_daddr;
    }

    if (udp->dest != new_dport) {
        if (udp->check) {
            zereh_csum_replace_u16(&udp->check, udp->dest, new_dport);
        }
        udp->dest = new_dport;
    }

    return XDP_PASS;
}

static __always_inline int zereh_dispatch(struct xdp_md *ctx,
                                          struct iphdr *iph,
                                          struct udphdr *udp,
                                          const struct dns_query_meta *qmeta)
{
    __u64 route_hash = zereh_route_hash(qmeta);

    switch (route_hash) {
    /*__ZEREH_ROUTE_CASES__*/
    default:
        break;
    }

    /*__ZEREH_FALLBACK_RULES__*/

    return ZEREH_DEFAULT_ACTION;
}

SEC("xdp")
int xdp_router(struct xdp_md *ctx)
{
    void *data = (void *)(long)ctx->data;
    void *data_end = (void *)(long)ctx->data_end;
    struct ethhdr *eth;
    struct iphdr *iph;
    struct udphdr *udp;
    struct dns_query_meta qmeta;
    __u32 packet_len;

    if (zereh_parse_eth_ipv4_udp(data, data_end, &eth, &iph, &udp) != 0) {
        return XDP_PASS;
    }

    if (udp->dest != bpf_htons(53)) {
        return XDP_PASS;
    }

    if (zereh_parse_dns_query(data_end, udp, &qmeta) != 0) {
        return XDP_PASS;
    }

    packet_len = (__u32)((unsigned long)data_end - (unsigned long)data);
    if (zereh_global_pre_route(iph, &qmeta, packet_len) != 0) {
        return XDP_DROP;
    }

    return zereh_dispatch(ctx, iph, udp, &qmeta);
}

SEC("xdp_app_default")
int zereh_app_default(struct xdp_md *ctx)
{
    (void)ctx;
    return XDP_PASS;
}
