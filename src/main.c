
#include <nrfx.h>

#include <zephyr.h>
#include <device.h>

#include <sys/util.h>
#include <sys/printk.h>
#include <inttypes.h>
#include <logging/log.h>

#include <date_time.h>

#include "common.h"
#include "ov7675.h"
#include "sd.h"
#include "ftp.h"

#define SLEEP_TIME_MS	1

/*************** TODO *******************************
-automatically make directories on sd card and ftp
-figure out how to name files when no network info
-add printing of where the time is from into status

****************************************************/


LOG_MODULE_REGISTER(main);

const struct device * gpio; 
const struct device * i2c_sccb;

uint8_t image_buffer[IMAGE_SIZE_BYTES];


static struct master_config_t mcfg;
struct capture_t capture = {.data = image_buffer, .length = IMAGE_SIZE_BYTES, .time = 0};
struct status_t stats = {.system_time = 0, .battery_voltage = -1, .captures = 0};

int sleepy(uint32_t ms_sleep){
	return k_msleep(ms_sleep);
}

int get_capture_time(int32_t* ct){
	int ret;
	int d = mcfg.trig_cfg.logging_decimation_ftp;	
	
	if(d>0){//for when ftp is enabled
	uint64_t unix_time_ms; 
	ret = date_time_now(&unix_time_ms);
	if(ret < 0){return ret;}
	*ct = (uint32_t) (unix_time_ms/1000);
	}
	return 0;
}

int update_status(){
	stats.system_time = capture.time; 
	stats.battery_voltage = -1;//fix this
	
	return 0;
}

int setup(void){
	int ret;	
	int d = mcfg.trig_cfg.logging_decimation_ftp;	

	ret = sdhc_mount();
	
	ret = sdhc_load_config("/config.txt", &mcfg);
	if(ret < 0){
		LOG_ERR("failed to load config. halting");
		return -1;
	}
	
	if(d > 0){
		ftp_setup();
		//gets current network time
		ret = modem_network_register(&mcfg.ftp_cfg);
		if (ret < 0){return ret;}
		
		date_time_update_async(NULL);
		LOG_INF("busy wait for valid time");
		for(uint32_t i = 0; i < 1000; i++){
			if(date_time_is_valid()){break;}
			k_msleep(10);
		}
	}else{
		struct tm cal;
		ret = sdhc_load_last_status_time(mcfg.sd_cfg.status_path, &cal);
		if(ret < 0){return ret;}
		ret = date_time_set(&cal);
		if(ret < 0){return ret;}
	}
	
	
	
	return 0;
}

int loop(void){
	int d = mcfg.trig_cfg.logging_decimation_ftp;
	
	get_capture_time(&capture.time);
	update_status();
	
	LOG_INF("ov7675 initialisation");
	ov7675_init(mcfg.im_cfg.auto_range_time);

	LOG_INF("ov7675 capture");
	ov7675_capture(capture.data);
	
	LOG_INF("image -> sdhc");
	sdhc_write_image(mcfg.sd_cfg.image_path, 
					 &capture);
	sdhc_write_status(mcfg.sd_cfg.status_path, 
					 &stats);
					 
	if ((d > 0) && (0 == (stats.captures % d))){
		LOG_INF("image -> ftp");
		ftp_write_image(&mcfg.ftp_cfg, 
						&capture);
		ftp_write_status(&mcfg.ftp_cfg, 
					 &stats);
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
	
	stats.captures++;
	
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
