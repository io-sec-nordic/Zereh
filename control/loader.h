#ifndef ZEREH_LOADER_H
#define ZEREH_LOADER_H

#include "config.h"

int zereh_load_and_swap_xdp(const struct zereh_config *cfg, const char *ifname_override);
int zereh_detach_xdp(const struct zereh_config *cfg, const char *ifname_override);

#endif /* ZEREH_LOADER_H */
