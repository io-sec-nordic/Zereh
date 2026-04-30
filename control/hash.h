#ifndef ZEREH_HASH_H
#define ZEREH_HASH_H

#include "config.h"

#include <stdint.h>

#define ZEREH_FNV1A64_OFFSET 1469598103934665603ULL
#define ZEREH_FNV1A64_PRIME 1099511628211ULL

uint64_t zereh_fnv1a64_domain(const char *domain);
uint64_t zereh_siphash24_domain(const char *domain);
uint64_t zereh_hash_domain(const char *domain, enum zereh_hash_algorithm algo);

#endif /* ZEREH_HASH_H */
