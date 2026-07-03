#pragma once

typedef struct {
    uint8_t unit;
    uint8_t value;
} psm_timer_t;

int modem_init(void);

int modem_shutdown(void);

int slm_vbat(int* bat_mv);


int modem_network_register(const struct ftp_config_t* ftp_cfg_p);
int modem_network_deregister(void);