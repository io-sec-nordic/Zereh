#ifndef ZEREH_LICENSE_H
#define ZEREH_LICENSE_H

#if __has_include("zereh_build_info.h")
#include "zereh_build_info.h"
#else
#define ZEREH_BUILD_VERSION "0.1.0-dev"
#define ZEREH_BUILD_REVISION "nogit"
#define ZEREH_BUILD_TIMESTAMP_UTC "unknown"
#endif

/*
 * Zereh - eBPF/XDP Zero-Latency DNS Router & Edge Filter
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Commercial licensing is available from IO-SEC Nordic AB.
 * This placeholder contact should be replaced before distribution.
 */
#define ZEREH_LICENSE_SPDX "AGPL-3.0-or-later"
#define ZEREH_COMMERCIAL_LICENSE_HOLDER "IO-SEC Nordic AB"
#define ZEREH_COMMERCIAL_LICENSE_CONTACT "commercial-license@io-sec.eu"

#endif /* ZEREH_LICENSE_H */
