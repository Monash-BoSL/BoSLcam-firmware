#pragma once

int lsdir(const char* path);
int fs_mkdirs(const char* path);
void sdhc_info(void);
int sdhc_mount(void);
int sdhc_move_image(const char* sdhc_path, struct capture_t *);
int sdhc_write_image(const char* sdhc_path, const struct capture_t *);
int sdhc_write_status(const char* sdhc_path, const struct status_t* status);
int sdhc_load_config(const char* sdhc_path, struct master_config_t* master_cfg);
int sdhc_load_last_status_time(const char* sdhc_path, struct tm* cal);
int sdhc_file_to_rtt(const char* sdhc_path);