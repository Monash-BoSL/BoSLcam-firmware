
#include <nrfx.h>

#include <zephyr.h>
#include <device.h>

#include <sys/util.h>
#include <sys/printk.h>
#include <inttypes.h>
#include <logging/log.h>

#include "config.h"
#include "ov7675.h"
#include "sd.h"
#include "ftp.h"

#define SLEEP_TIME_MS	1

LOG_MODULE_REGISTER(main);

const struct device * gpio; 
const struct device * i2c_sccb;

uint8_t image_buffer[IMAGE_SIZE_BYTES];

struct image_config_t {
	uint32_t auto_range_time;
	//image size 
	//awb enable
	//ae enable
	//...
};

struct ftp_config_t {
	char* apn;
	char* network_operator;
	char* domain;
	//port
	char* username;
	char* password;
	char* image_path;
	char* status_path;
};

struct sd_config_t {
	char* image_path;
	char* status_path;
	int	logging_level;
};

enum trigger_type {
	TIME_TIRGGER,
	UART_TRIGGER,
};

struct trigger_config_t {
	enum trigger_type trig_type;
	uint32_t logging_interval_ftp;
	uint32_t logging_interval_sd;
}

struct master_config_t {
	//struct gnss_config;
	struct trigger_config_t trig_cfg;
	struct image_config_t im_cfg;
	struct ftp_config_t ftp_cfg;
	struct sd_config_t sd_cfg;
};

struct master_config_t master_cfg;

int setup(void){
	ret = sdhc_load_config("/config.txt", master_cfg);
	if(ret < 0){
		LOG_ERR("failed to load config. halting");
		return -1;
	}
}

int loop(void){
	LOG_INF("ov7675 initialisation");
	ov7675_init(1000);

	LOG_INF("ov7675 capture");
	ov7675_capture(image_buffer);

	LOG_INF("image -> sdhc");
	sdhc_write_image("/1.bmp", image_buffer, IMAGE_SIZE_BYTES);
	
	LOG_INF("image -> ftp");
	ftp_write_image("/1.bmp", image_buffer, IMAGE_SIZE_BYTES);
	
	LOG_INF("done");
	while(1){
		k_msleep(100);
	}
}

void main(void){
	int ret;
	
	LOG_INF("begin!");
	ret = setup();
	if(ret < 0){
		//lockup program and halt
		//try call for help
	}
	
	while(1){
		loop();
	}
}
