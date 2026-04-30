#ifndef ZEREH_DNS_PARSER_H
#define ZEREH_DNS_PARSER_H

#include <stdbool.h>

#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/udp.h>

#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>

#define ZEREH_DNS_MAX_LABELS 32
#define ZEREH_DNS_MAX_LABEL_LEN 63
#define ZEREH_DNS_MAX_QNAME 255

struct zereh_dns_hdr {
    __be16 id;
    __be16 flags;
    __be16 qdcount;
    __be16 ancount;
    __be16 nscount;
    __be16 arcount;
} __attribute__((packed));

struct dns_query_meta {
    __u64 qname_hash;
    __u16 dns_id;
    __u16 qtype;
    __u16 qclass;
    __u8 opcode;
    __u8 is_truncated;
    __u8 label_depth;
    __u16 qname_len;
    char qname[ZEREH_DNS_MAX_QNAME + 1];
};

static __always_inline __u8 zereh_ascii_tolower(__u8 c)
{
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

static __always_inline __u64 zereh_fnv1a64_step(__u64 hash, __u8 c)
{
    return (hash ^ c) * 1099511628211ULL;
}

static __always_inline int zereh_parse_eth_ipv4_udp(void *data,
                                                     void *data_end,
                                                     struct ethhdr **eth,
                                                     struct iphdr **iph,
                                                     struct udphdr **udp)
{
    __u32 ihl_bytes;

    *eth = data;
    if ((void *)(*eth + 1) > data_end) {
        return -1;
    }

    if ((*eth)->h_proto != bpf_htons(ETH_P_IP)) {
        return -1;
    }

    *iph = (void *)(*eth + 1);
    if ((void *)(*iph + 1) > data_end) {
        return -1;
    }

    if ((*iph)->ihl < 5 || (*iph)->protocol != IPPROTO_UDP) {
        return -1;
    }

    ihl_bytes = (__u32)(*iph)->ihl * 4;
    if ((void *)(*iph) + ihl_bytes > data_end) {
        return -1;
    }

    *udp = (void *)(*iph) + ihl_bytes;
    if ((void *)(*udp + 1) > data_end) {
        return -1;
    }

    return 0;
}

static __always_inline int zereh_parse_dns_query(void *data_end,
                                                  const struct udphdr *udp,
                                                  struct dns_query_meta *meta)
{
    const struct zereh_dns_hdr *dns;
    const unsigned char *dns_end;
    const unsigned char *cursor;
    __u16 flags;
    bool ended = false;
    int label;

    if (!udp || !meta) {
        return -1;
    }

    dns = (const struct zereh_dns_hdr *)(udp + 1);
    if ((const void *)(dns + 1) > data_end) {
        return -1;
    }

    dns_end = (const unsigned char *)udp + bpf_ntohs(udp->len);
    if ((const void *)dns_end > data_end) {
        return -1;
    }

    if (dns_end < (const unsigned char *)(dns + 1) + 5) {
        return -1;
    }

    flags = bpf_ntohs(dns->flags);
    if ((flags & 0x8000) != 0) {
        return -1;
    }

    if (bpf_ntohs(dns->qdcount) == 0) {
        return -1;
    }

    __builtin_memset(meta, 0, sizeof(*meta));
    meta->dns_id = bpf_ntohs(dns->id);
    meta->opcode = (flags >> 11) & 0x0f;
    meta->is_truncated = (flags & 0x0200) ? 1 : 0;
    meta->qname_hash = 1469598103934665603ULL;

    cursor = (const unsigned char *)(dns + 1);

#pragma clang loop unroll(disable)
    for (label = 0; label < ZEREH_DNS_MAX_LABELS; label++) {
        __u8 lbl_len;
        int j;

        if (cursor + 1 > dns_end) {
            return -1;
        }

        lbl_len = *cursor;
        cursor++;

        if (lbl_len == 0) {
            ended = true;
            break;
        }

        if ((lbl_len & 0xc0) != 0 || lbl_len > ZEREH_DNS_MAX_LABEL_LEN) {
            return -1;
        }

        if (cursor + lbl_len > dns_end) {
            return -1;
        }

        if (meta->label_depth > 0) {
            if (meta->qname_len >= ZEREH_DNS_MAX_QNAME) {
                return -1;
            }
            meta->qname[meta->qname_len++] = '.';
            meta->qname_hash = zereh_fnv1a64_step(meta->qname_hash, '.');
        }

#pragma clang loop unroll(disable)
        for (j = 0; j < ZEREH_DNS_MAX_LABEL_LEN; j++) {
            __u8 c;

            if (j >= lbl_len) {
                break;
            }

            c = zereh_ascii_tolower(cursor[j]);

            if (meta->qname_len >= ZEREH_DNS_MAX_QNAME) {
                return -1;
            }

            meta->qname[meta->qname_len++] = (char)c;
            meta->qname_hash = zereh_fnv1a64_step(meta->qname_hash, c);
        }

        cursor += lbl_len;
        meta->label_depth++;
    }

    if (!ended || meta->qname_len == 0) {
        return -1;
    }

    if (cursor + 4 > dns_end) {
        return -1;
    }

    {
        __be16 qtype_be;
        __be16 qclass_be;

        __builtin_memcpy(&qtype_be, cursor, sizeof(qtype_be));
        cursor += sizeof(qtype_be);

        __builtin_memcpy(&qclass_be, cursor, sizeof(qclass_be));

        meta->qtype = bpf_ntohs(qtype_be);
        meta->qclass = bpf_ntohs(qclass_be);
    }

    meta->qname[meta->qname_len] = '\0';
    return 0;
}

#endif /* ZEREH_DNS_PARSER_H */
