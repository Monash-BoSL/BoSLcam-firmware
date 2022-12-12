#pragma once

int lsdir(const char* path);
void sdhc_info(void);
int sdhc_mount(void);
int sdhc_write_image(char* sdhc_path, struct capture_t *);
int sdhc_load_config(char* sdhc_path, struct master_config_t* mcfg);