
#include <zephyr.h>
#include <device.h>


#include <sys/util.h>
#include <sys/printk.h>
#include <inttypes.h>
#include <logging/log.h>

#include <nrf_modem.h>
#include <nrf_modem_at.h>
#include <modem/nrf_modem_lib.h>
#include <modem/at_monitor.h>

#include <net/ftp_client.h>

#include <date_time.h>

#include <stdio.h>

#include "common.h"
#include "ftp.h"

LOG_MODULE_REGISTER(ftp);

void ftp_data_callback(const uint8_t *msg, uint16_t len)
{
	printk(msg);
}

void ftp_ctrl_callback(const uint8_t *msg, uint16_t len)
{
	printk(msg);
}

int ftp_write_image(struct ftp_config_t* ftp_cfg_p, char* data, uint32_t length){
	const uint32_t max_path_length = 256;
	char path[256];
	int ret;
	
	LOG_INF("modem begin\n");
	
	if(strlen(ftp_cfg_p->image_path) > max_path_length + 12-1){//12 for unix time + extension
		LOG_ERR("file name too long");
		return -ENAMETOOLONG;
	}
	uint64_t unix_time_ms; 
	ret = date_time_now(&unix_time_ms);
	if(ret < 0){return ret;}
	uint32_t unix_time_s = (uint32_t) unix_time_ms/1000;
	
	sprintf(path, "%s%08X.bmp", ftp_cfg_p->image_path, unix_time_s);
	
	
	// ret = nrf_modem_at_cmd(response, sizeof(response), "AT+CGMR");
	// printk(response);
	
	ret = nrf_modem_at_printf("AT");
	if(ret == 0){LOG_INF("AT initialised");}
	else if (ret < 0){LOG_ERR("AT initialisation error"); return ret;}
	
	ret = nrf_modem_at_printf("AT+CGDCONT=0,\"IP\",\"%s\"", ftp_cfg_p->apn);
	if(ret == 0){LOG_INF("CGDCONT ok");}
	else if (ret < 0){LOG_ERR("CGDCONT error"); return ret;}
	
	ret = nrf_modem_at_printf("AT+CFUN=1");
	if(ret == 0){LOG_INF("CFUN on ok");}
	else if (ret < 0){LOG_ERR("CFUN on error"); return ret;}
	
	ret = nrf_modem_at_printf("AT+COPS=1,2,\"%s\"", ftp_cfg_p->network_operator);
	if(ret == 0){LOG_INF("COPS register ok");}
	else if (ret < 0){LOG_ERR("COPS register error"); return ret;}
	
	// ret = nrf_modem_at_cmd(response, sizeof(response), "AT+CEREG?");
	// printk(response);
	
	// ret = nrf_modem_at_cmd(response, sizeof(response), "AT+CPIN?");
	// printk(response);
	
	
	
	ftp_init(ftp_ctrl_callback, ftp_data_callback);
	ret = ftp_open(ftp_cfg_p->domain, 21, -1);
	ret = ftp_login(ftp_cfg_p->username, ftp_cfg_p->password);
	ret = ftp_type(FTP_TYPE_BINARY);
	ret = ftp_put(path, bmp_header, BMPIMAGEOFFSET, FTP_PUT_NORMAL);
	ret = ftp_put(path, data, length, FTP_PUT_APPEND);
	// ftp status
	ret = ftp_close();
	LOG_INF("UPLOAD SEQUENCE ENDED");
	return 0;
}