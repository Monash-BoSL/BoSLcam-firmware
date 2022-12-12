
#include <nrfx.h>

#include <zephyr.h>
#include <device.h>

#include <sys/util.h>
#include <sys/printk.h>
#include <inttypes.h>
#include <logging/log.h>

#include "common.h"
#include "ov7675.h"
#include "sd.h"
#include "ftp.h"

#define SLEEP_TIME_MS	1

LOG_MODULE_REGISTER(main);

const struct device * gpio; 
const struct device * i2c_sccb;

uint8_t image_buffer[IMAGE_SIZE_BYTES];

static struct master_config_t mcfg;
uint32_t captures = 0;

int sleepy(uint32_t ms_sleep){
	return k_msleep(ms_sleep);
}

int setup(void){
	int ret;
	ret = sdhc_load_config("/config.txt", &mcfg);
	if(ret < 0){
		LOG_ERR("failed to load config. halting");
		return -1;
	}
	return 0;
}

int loop(void){
	
	// status();
	
	LOG_INF("ov7675 initialisation");
	ov7675_init(mcfg.im_cfg.auto_range_time);

	LOG_INF("ov7675 capture");
	ov7675_capture(image_buffer);

	LOG_INF("image -> sdhc");
	sdhc_write_image(mcfg.sd_cfg.image_path, 
					 image_buffer, 
					 IMAGE_SIZE_BYTES);
					 
	int d = mcfg.trig_cfg.logging_decimation_ftp;
	if ((d > 0) && (0 == (captures % d))){
		LOG_INF("image -> ftp");
		ftp_write_image(&mcfg.ftp_cfg, 
						image_buffer, 
						IMAGE_SIZE_BYTES);
	}
	LOG_INF("done");
	
	switch (mcfg.trig_cfg.trig_type){
	case TIME_TIRGGER:
		LOG_INF("time sleep");
		sleepy(mcfg.trig_cfg.logging_interval);
		break;		
	case UART_TRIGGER:
		LOG_INF("uart sleep");
		// uart_sei();
		sleepy(0);
		// uart_cli();
		break;
	default:
		LOG_ERR("trig_type misconfigred, oops!");
		k_oops();
		break;	
	}
	
	captures++;
	
	return 0;
}

void main(void){
	int ret;
	
	LOG_INF("begin!");
	ret = setup();
	if(ret < 0){
		k_oops();
		//lockup program and halt
		//try call for help
	}
	
	while(1){
		loop();
	}
	
	while(1){
		k_msleep(100);
	}
}
