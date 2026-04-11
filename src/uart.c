#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>

#include "common.h"

LOG_MODULE_REGISTER(uart);

#define ARG_IS_X   (-1)
#define MAX_BUF 32

static char buf[MAX_BUF];
static int idx = 0;

void parser_reset(void){idx = 0;}

int is_valid_cmd(const char c) {
    return (
                c == 'I' 
            ||  c == 'D' 
            ||  c == 'R'
        );
}

void process_buffer(struct master_config_t* const ccfg, const struct master_config_t* const mcfg) {
    if (idx < 2) {
        LOG_ERR("command too short");
        parser_reset();
        return;
    }

    char cmd = buf[0];

    if (!is_valid_cmd(cmd)) {
        LOG_ERR("invalid command");
        parser_reset();
        return;
    }

    // case: X argument
    if (buf[1] == 'X' && idx == 2) {
        handle_command(cmd, ARG_IS_X, ccfg, mcfg);
        parser_reset();
        return;
    }

    // case: integer argument
    int value = 0;
    for (int i = 1; i < idx; i++) {
        if (!isdigit((unsigned char)buf[i])) {
            LOG_ERR("invalid character");
            parser_reset();  // invalid character
            return;
        }
        value = value * 10 + (buf[i] - '0');
    }

    handle_command(cmd, value, ccfg, mcfg);
    parser_reset();
}

void process_input(uint8_t c, struct master_config_t* const ccfg, const struct master_config_t* const mcfg) {
    if (c == ';') {
        process_buffer(ccfg, mcfg);  // end of command
        return;
    }

    if (idx >= MAX_BUF - 1) {
        LOG_ERR("buffer overflow");
        parser_reset(); // overflow protection
        return;
    }

    buf[idx++] = c;
}


void handle_command(const char cmd, const int value, struct master_config_t* const ccfg, const struct master_config_t* const mcfg) {
    switch (cmd) {
        case 'I':
            ccfg->trig_cfg.logging_interval = (value == ARG_IS_X) ? mcfg->trig_cfg.logging_interval : value;
            break;

        case 'D':
            ccfg->trig_cfg.logging_decimation_ftp = (value == ARG_IS_X) ? mcfg->trig_cfg.logging_decimation_ftp : value;
            break;

        case 'R':
            ccfg->im_cfg.resolution = (value == ARG_IS_X) ? mcfg->im_cfg.resolution : value;
            break;

        default:
            LOG_ERR("invalid command %c", cmd);
            break;
        }
}