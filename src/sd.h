#pragma once

int lsdir(const char* path);
int fs_mkdirs(const char* path);
void sdhc_info(void);
int sdhc_mount(void);
int sdhc_move_image(char* sdhc_path, struct capture_t *);
int sdhc_write_image(char* sdhc_path, struct capture_t *);
int sdhc_write_status(char* sdhc_path, struct status_t* status);
int sdhc_load_config(char* sdhc_path, struct master_config_t* mcfg);
int sdhc_load_last_status_time(char* sdhc_path, struct tm* cal);
int sdhc_file_to_rtt(char* sdhc_path);