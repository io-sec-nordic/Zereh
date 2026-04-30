#include "codegen.h"
#include "compiler.h"
#include "config.h"
#include "hash.h"
#include "zereh_license.h"
#include "loader.h"
#include "yaml_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static void zereh_usage(const char *prog)
{
    fprintf(stderr,
            "Usage:\n"
            "  %s generate -c <config.yaml>\n"
            "  %s load -c <config.yaml> [-i ifname]\n"
            "  %s unload -c <config.yaml> [-i ifname]\n"
            "  %s generate-load -c <config.yaml> [-i ifname]\n"
            "  %s hash <domain> [fnv1a|siphash]\n"
            "  %s version\n",
            prog,
            prog,
            prog,
            prog,
            prog,
            prog);
}

int main(int argc, char **argv)
{
    struct zereh_config *cfg = NULL;
    const char *cmd;
    const char *config_path = "config.yaml";
    const char *ifname_override = NULL;
    int i;

    if (argc < 2) {
        zereh_usage(argv[0]);
        return 1;
    }

    cmd = argv[1];

    if (strcmp(cmd, "hash") == 0) {
        enum zereh_hash_algorithm algo = ZEREH_HASH_FNV1A;

        if (argc < 3) {
            zereh_usage(argv[0]);
            return 1;
        }

        if (argc >= 4) {
            if (strcasecmp(argv[3], "siphash") == 0) {
                algo = ZEREH_HASH_SIPHASH;
            } else if (strcasecmp(argv[3], "fnv1a") == 0 ||
                       strcasecmp(argv[3], "fnv-1a") == 0) {
                algo = ZEREH_HASH_FNV1A;
            } else {
                fprintf(stderr, "unknown hash algorithm: %s\n", argv[3]);
                return 1;
            }
        }

        printf("0x%016llx\n", (unsigned long long)zereh_hash_domain(argv[2], algo));
        return 0;
    }

    if (strcmp(cmd, "version") == 0) {
        printf("zereh version: %s\n", ZEREH_BUILD_VERSION);
        printf("zereh revision: %s\n", ZEREH_BUILD_REVISION);
        printf("zereh build timestamp (UTC): %s\n", ZEREH_BUILD_TIMESTAMP_UTC);
        printf("license: %s\n", ZEREH_LICENSE_SPDX);
        printf("commercial holder: %s\n", ZEREH_COMMERCIAL_LICENSE_HOLDER);
        printf("commercial contact: %s\n", ZEREH_COMMERCIAL_LICENSE_CONTACT);
        return 0;
    }

    cfg = calloc(1, sizeof(*cfg));
    if (!cfg) {
        fprintf(stderr, "failed to allocate config\n");
        return 1;
    }

    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && (i + 1) < argc) {
            config_path = argv[++i];
        } else if (strcmp(argv[i], "-i") == 0 && (i + 1) < argc) {
            ifname_override = argv[++i];
        } else {
            zereh_usage(argv[0]);
            free(cfg);
            return 1;
        }
    }

    zereh_config_init_defaults(cfg);
    if (strcmp(cmd, "unload") == 0) {
        if (zereh_parse_config_yaml(config_path, cfg) != 0) {
            if (!ifname_override) {
                fprintf(stderr, "failed to parse config: %s\n", config_path);
                fprintf(stderr, "unload requires either a valid config or explicit interface override (-i)\n");
                free(cfg);
                return 1;
            }
            fprintf(stderr,
                    "warning: failed to parse config (%s); proceeding with explicit interface override\n",
                    config_path);
        }

        if (zereh_detach_xdp(cfg, ifname_override) != 0) {
            fprintf(stderr, "unload failed\n");
            free(cfg);
            return 1;
        }
        free(cfg);
        return 0;
    }

    if (zereh_parse_config_yaml(config_path, cfg) != 0) {
        fprintf(stderr, "failed to parse config: %s\n", config_path);
        free(cfg);
        return 1;
    }

    if (strcmp(cmd, "generate") == 0) {
        if (zereh_generate_xdp_source(cfg) != 0) {
            fprintf(stderr, "generation failed\n");
            free(cfg);
            return 1;
        }
        if (zereh_compile_generated_bpf(cfg) != 0) {
            fprintf(stderr, "compile failed\n");
            free(cfg);
            return 1;
        }
        printf("generated and compiled: %s -> %s\n", cfg->generated_source, cfg->generated_object);
        free(cfg);
        return 0;
    }

    if (strcmp(cmd, "load") == 0) {
        if (zereh_load_and_swap_xdp(cfg, ifname_override) != 0) {
            fprintf(stderr, "load failed\n");
            free(cfg);
            return 1;
        }
        free(cfg);
        return 0;
    }

    if (strcmp(cmd, "generate-load") == 0) {
        if (zereh_generate_xdp_source(cfg) != 0) {
            fprintf(stderr, "generation failed\n");
            free(cfg);
            return 1;
        }
        if (zereh_compile_generated_bpf(cfg) != 0) {
            fprintf(stderr, "compile failed\n");
            free(cfg);
            return 1;
        }
        if (zereh_load_and_swap_xdp(cfg, ifname_override) != 0) {
            fprintf(stderr, "load failed\n");
            free(cfg);
            return 1;
        }
        free(cfg);
        return 0;
    }

    zereh_usage(argv[0]);
    free(cfg);
    return 1;
}
