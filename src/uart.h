#pragma once

#include <stdint.h>

#include "common.h"

void parser_reset(void);
void process_input(uint8_t c, struct master_config_t* const ccfg, const struct master_config_t* const mcfg);