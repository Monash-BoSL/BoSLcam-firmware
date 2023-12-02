#pragma once

void ftp_setup(void);
int modem_network_register(struct ftp_config_t* ftp_cfg_p);
int ftp_write_bmp(struct ftp_config_t* ftp_cfg_p, struct capture_t *, enum image_size im_size);
int ftp_write_jpg(struct ftp_config_t* ftp_cfg_p, struct capture_t *);
int ftp_write_status(struct ftp_config_t* ftp_cfg_p, struct status_t *);