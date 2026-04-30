#include "hash.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#define ZEREH_SIPHASH_K0 0x0706050403020100ULL
#define ZEREH_SIPHASH_K1 0x0f0e0d0c0b0a0908ULL

static uint64_t zereh_rotl64(uint64_t x, unsigned int b)
{
    return (x << b) | (x >> (64U - b));
}

static void zereh_sipround(uint64_t *v0, uint64_t *v1, uint64_t *v2, uint64_t *v3)
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

static size_t zereh_normalize_domain(const char *domain, char *out, size_t out_sz)
{
    size_t i = 0;
    size_t n = 0;

    if (!domain || !out || out_sz == 0) {
        return 0;
    }

    while (domain[i] != '\0' && n + 1 < out_sz) {
        unsigned char c = (unsigned char)domain[i];

        if (c == '.' && domain[i + 1] == '\0') {
            break;
        }

        out[n++] = (char)tolower(c);
        i++;
    }

    out[n] = '\0';
    return n;
}

static uint64_t zereh_fnv1a64_bytes(const unsigned char *data, size_t len)
{
    uint64_t hash = ZEREH_FNV1A64_OFFSET;
    size_t i;

    for (i = 0; i < len; i++) {
        hash ^= (uint64_t)data[i];
        hash *= ZEREH_FNV1A64_PRIME;
    }

    return hash;
}

static uint64_t zereh_siphash24_bytes(const unsigned char *data, size_t len)
{
    uint64_t v0 = 0x736f6d6570736575ULL ^ ZEREH_SIPHASH_K0;
    uint64_t v1 = 0x646f72616e646f6dULL ^ ZEREH_SIPHASH_K1;
    uint64_t v2 = 0x6c7967656e657261ULL ^ ZEREH_SIPHASH_K0;
    uint64_t v3 = 0x7465646279746573ULL ^ ZEREH_SIPHASH_K1;
    uint64_t b = ((uint64_t)len) << 56;
    size_t off = 0;
    size_t i;

    while (off + 8 <= len) {
        uint64_t m = ((uint64_t)data[off + 0]) |
                     ((uint64_t)data[off + 1] << 8) |
                     ((uint64_t)data[off + 2] << 16) |
                     ((uint64_t)data[off + 3] << 24) |
                     ((uint64_t)data[off + 4] << 32) |
                     ((uint64_t)data[off + 5] << 40) |
                     ((uint64_t)data[off + 6] << 48) |
                     ((uint64_t)data[off + 7] << 56);

        v3 ^= m;
        zereh_sipround(&v0, &v1, &v2, &v3);
        zereh_sipround(&v0, &v1, &v2, &v3);
        v0 ^= m;
        off += 8;
    }

    for (i = 0; i < (len - off); i++) {
        b |= ((uint64_t)data[off + i]) << (8U * (unsigned int)i);
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

uint64_t zereh_fnv1a64_domain(const char *domain)
{
    char normalized[ZEREH_MAX_DOMAIN_LEN + 1];
    size_t n;

    n = zereh_normalize_domain(domain, normalized, sizeof(normalized));
    if (n == 0) {
        return 0;
    }

    return zereh_fnv1a64_bytes((const unsigned char *)normalized, n);
}

uint64_t zereh_siphash24_domain(const char *domain)
{
    char normalized[ZEREH_MAX_DOMAIN_LEN + 1];
    size_t n;

    n = zereh_normalize_domain(domain, normalized, sizeof(normalized));
    if (n == 0) {
        return 0;
    }

    return zereh_siphash24_bytes((const unsigned char *)normalized, n);
}

uint64_t zereh_hash_domain(const char *domain, enum zereh_hash_algorithm algo)
{
    if (algo == ZEREH_HASH_SIPHASH) {
        return zereh_siphash24_domain(domain);
    }
    return zereh_fnv1a64_domain(domain);
}
