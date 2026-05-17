#pragma once

void ftp_setup(void);
int ftp_write_image(const struct ftp_config_t* ftp_cfg_p, const struct capture_t*);
int ftp_write_status(const struct ftp_config_t* ftp_cfg_p, const struct status_t*);