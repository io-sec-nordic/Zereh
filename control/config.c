#include "config.h"

#include <stdio.h>
#include <string.h>

void zereh_config_init_defaults(struct zereh_config *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->interface, sizeof(cfg->interface), "%s", "eth0");
    snprintf(cfg->template_path, sizeof(cfg->template_path), "%s", "xdp/xdp_template.c");
    snprintf(cfg->generated_source, sizeof(cfg->generated_source), "%s", "build/xdp/router_generated.c");
    snprintf(cfg->generated_object, sizeof(cfg->generated_object), "%s", "build/xdp/router_generated.o");
    snprintf(cfg->xdp_section, sizeof(cfg->xdp_section), "%s", "xdp_router");
    cfg->xdp_mode = ZEREH_XDP_MODE_NATIVE;
    cfg->default_action = ZEREH_ACTION_PASS;
    cfg->max_packet_size = 1400;
    cfg->default_kernel_port = 5302;
    cfg->allowed_opcode_mask = (1u << 0);
    cfg->drop_truncated = 0;
    snprintf(cfg->hash_algorithm, sizeof(cfg->hash_algorithm), "%s", "FNV-1a");
    cfg->hash_algo = ZEREH_HASH_FNV1A;
    cfg->optimize_jump_tables = 1;
    cfg->inline_checksums = 1;
}
