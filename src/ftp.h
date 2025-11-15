#pragma once

void ftp_setup(void);
int ftp_write_image(struct ftp_config_t* ftp_cfg_p, struct capture_t*);
// int ftp_write_jpg(struct ftp_config_t* ftp_cfg_p, struct capture_t*);
int ftp_write_status(struct ftp_config_t* ftp_cfg_p, struct status_t*);