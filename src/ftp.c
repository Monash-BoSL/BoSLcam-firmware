
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

#include "config.h"
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

void ftp_write_image(char* path, char* data, uint32_t length){
	int ret;
	char response[256];
	
	LOG_INF("AT modem begin\n");
	
	// ret = nrf_modem_at_cmd(response, sizeof(response), "AT+CGMR");
	// printk(response);
	
	ret = nrf_modem_at_cmd(response, sizeof(response), "AT");
	printk(response);
	
	ret = nrf_modem_at_cmd(response, sizeof(response), "AT+CGDCONT=0,\"IP\",\"hologram\"");
	printk(response);
	
	ret = nrf_modem_at_cmd(response, sizeof(response), "AT+CFUN=1");
	printk(response);
		
	ret = nrf_modem_at_cmd(response, sizeof(response), "AT+COPS=1,2,\"50501\"");
	printk(response);
	
	// ret = nrf_modem_at_cmd(response, sizeof(response), "AT+CEREG?");
	// printk(response);
	
	// ret = nrf_modem_at_cmd(response, sizeof(response), "AT+CPIN?");
	// printk(response);
	
	
	ftp_init(ftp_ctrl_callback, ftp_data_callback);
	ret = ftp_open("ftp.bosl.com.au", 21, -1);
	ret = ftp_login("images@bosl.com.au", "solderflux");
	ret = ftp_type(FTP_TYPE_BINARY);
	ret = ftp_put(path, bmp_header, BMPIMAGEOFFSET, FTP_PUT_NORMAL);
	ret = ftp_put(path, data, length, FTP_PUT_APPEND);
	ret = ftp_close();
	LOG_INF("UPLOAD SEQUENCE ENDED");
}