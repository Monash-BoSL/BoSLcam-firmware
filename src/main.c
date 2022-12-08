/*
 * Copyright (c) 2016 Open-RnD Sp. z o.o.
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <nrfx.h>
#include <hal/nrf_timer.h>
#include <hal/nrf_dppi.h>
#include <hal/nrf_gpiote.h>
#include <hal/nrf_gpio.h>

#include <zephyr.h>
#include <device.h>

#include <storage/disk_access.h>
#include <fs/fs.h>
#include <ff.h>

#include <drivers/gpio.h>
#include <sys/util.h>
#include <sys/printk.h>
#include <inttypes.h>
#include <logging/log.h>

#include <nrf_modem.h>
#include <nrf_modem_at.h>
#include <modem/nrf_modem_lib.h>
#include <modem/at_monitor.h>

#include <net/ftp_client.h>

#include <stdio.h>
#include <stdlib.h>

#include <drivers/i2c.h>

#include "ov7675.h"

// #include <i2c.h>

#define SLEEP_TIME_MS	1


#define CAMADDR_WR  0x42
#define CAMADDR_RD  0x43
#define CAMADDR		0x21

#define SCCB_VS		11
#define SCCB_HREF	12
#define SCCB_PCLK	13
#define SCCB_XCLK	14
#define SCCB_PEN	15
#define SCCB_PDN	16

#define SCCB_CLK_DPPI_CH 0
#define GPIOTE_CLK_TSK 0

#define IMAGE_WIDTH			320
#define IMAGE_HEIGHT		240
#define IMAGE_SIZE_BYTES 	(IMAGE_WIDTH*IMAGE_HEIGHT*2)

LOG_MODULE_REGISTER(main);

static const struct device * gpio; 

const struct device * i2c_sccb;

uint8_t imbuf[IMAGE_SIZE_BYTES];

static FATFS fat_fs;
/* mounting info */
static struct fs_mount_t mp = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
};

/*
*  Note the fatfs library is able to mount only strings inside _VOLUME_STRS
*  in ffconf.h
*/
static const char *disk_mount_pt = "/SD:";


void sccb_setup(void){
	gpio = device_get_binding(DT_LABEL(DT_NODELABEL(gpio0)));
	LOG_INF("bind %s\n", gpio->name);
	
	gpio_pin_configure(gpio, SCCB_PEN, GPIO_OUTPUT);
	gpio_pin_configure(gpio, SCCB_PDN, GPIO_OUTPUT);
	for(int i = 0; i < 8; i++){
		gpio_pin_configure(gpio, i, GPIO_INPUT);
	}
	gpio_pin_configure(gpio, SCCB_VS, GPIO_INPUT);
	gpio_pin_configure(gpio, SCCB_HREF, GPIO_INPUT);
	gpio_pin_configure(gpio, SCCB_PCLK, GPIO_INPUT);
	gpio_pin_set_raw(gpio, SCCB_PEN, 1);
	gpio_pin_set_raw(gpio, SCCB_PDN, 0);
				
	NRF_P0->PIN_CNF[SCCB_XCLK] = 0b00000000000000000000000000000001; 


	//setup clock on XCLK pin at 8 MHz.
	nrf_timer_frequency_set(NRF_TIMER0, NRF_TIMER_FREQ_16MHz);
	nrf_timer_mode_set(NRF_TIMER0, NRF_TIMER_MODE_TIMER);
	nrf_timer_cc_set(NRF_TIMER0,NRF_TIMER_CC_CHANNEL0, 0x01);
	nrf_timer_bit_width_set(NRF_TIMER0, NRF_TIMER_BIT_WIDTH_8);
	nrf_timer_shorts_set(NRF_TIMER0, 0b00000000000000000000000000000001);//reset timer on match with compare 0
	nrf_timer_publish_set(NRF_TIMER0, NRF_TIMER_EVENT_COMPARE0,SCCB_CLK_DPPI_CH);
	nrf_gpiote_subscribe_set(NRF_GPIOTE, NRF_GPIOTE_TASK_OUT_0 ,SCCB_CLK_DPPI_CH);
	nrf_gpiote_task_configure(NRF_GPIOTE,
								 GPIOTE_CLK_TSK,
								 SCCB_XCLK,
								 NRF_GPIOTE_POLARITY_TOGGLE,
								 NRF_GPIOTE_INITIAL_VALUE_HIGH);
    nrf_gpiote_task_enable(NRF_GPIOTE,GPIOTE_CLK_TSK);


	nrf_timer_task_trigger(NRF_TIMER0,NRF_TIMER_TASK_START);

    /* Enable DPPI Channel */
    nrf_dppi_channels_enable(NRF_DPPIC, 0x01 << SCCB_CLK_DPPI_CH);


	
	i2c_sccb = device_get_binding(DT_LABEL(DT_NODELABEL(i2c2)));
	LOG_INF("bind %s\n", i2c_sccb->name);

	camInit();	
	wrReg(0x11,0);//set clock divider to 1, no need to slow it down!
	

	k_msleep(1000);//delay for autoexposure awb, etc ...
}

void get_frame(void){
	uint16_t wg = IMAGE_WIDTH;//line width in pixels
	uint16_t hg = IMAGE_HEIGHT;//number of lines per frame
	uint16_t lg2;
	uint32_t p = 0;

	LOG_INF("RDY\n");
	//Wait for vsync 
	while(!nrf_gpio_pin_read(SCCB_VS));//wait for high
	while(nrf_gpio_pin_read(SCCB_VS));//wait for low

	while(hg--){//get line
		lg2=wg;
		// printk("%u\n", hg);
		while(lg2--){//get pixel
			//low byte
			while(NRF_P0->IN & (0x1 << SCCB_PCLK));//wait for high on PCLK
			imbuf[p+1] = (uint8_t) NRF_P0->IN;//read in D0 - D8
			while(!(NRF_P0->IN & (0x1 << SCCB_PCLK)));//wait for low on PCLK
			//high byte
			while(NRF_P0->IN & (0x1 << SCCB_PCLK));//wait for high on PCLK
			imbuf[p] = (uint8_t) NRF_P0->IN;//read in D0 - D8
			while(!(NRF_P0->IN & (0x1 << SCCB_PCLK)));//wait for low on PCLK
			
			p += 2;
		}
		while((nrf_gpio_pin_read(SCCB_HREF)));//SYNC line on HREF
	}

	//due to hardware error we need to swap the last 2 bits of imbuf
	for(uint32_t p = 0; p < IMAGE_SIZE_BYTES; p++){
		uint8_t x = imbuf[p];
		
		imbuf[p] = (x & ~(0x3)) | ((x >> 0x1)&0x1) | ((x << 1)&0x2);
	}
}



static int lsdir(const char *path)
{
	int res;
	struct fs_dir_t dirp;
	static struct fs_dirent entry;

	fs_dir_t_init(&dirp);

	/* Verify fs_opendir() */
	res = fs_opendir(&dirp, path);
	if (res) {
		LOG_ERR("Error opening dir %s [%d]\n", path, res);
		return res;
	}

	LOG_INF("\nListing dir %s ...\n", path);
	for (;;) {
		/* Verify fs_readdir() */
		res = fs_readdir(&dirp, &entry);

		/* entry.name[0] == 0 means end-of-dir */
		if (res || entry.name[0] == 0) {
			break;
		}

		if (entry.type == FS_DIR_ENTRY_DIR) {
			LOG_INF("[DIR ] %s\n", entry.name);
		} else {
			LOG_INF("[FILE] %s (size = %zu)\n",
				entry.name, entry.size);
		}
	}

	/* Verify fs_closedir() */
	fs_closedir(&dirp);

	return res;
}

#define BMPIMAGEOFFSET 66
const char bmp_header[BMPIMAGEOFFSET] =
{
  0x42, 0x4D, 0x36, 0x58, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00, 0x28, 0x00,
  0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00, 0x03, 0x00,
  0x00, 0x00, 0x00, 0x58, 0x02, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0xE0, 0x07, 0x00, 0x00, 0x1F, 0x00,
  0x00, 0x00
};

void ftp_data_callback(const uint8_t *msg, uint16_t len)
{
	// LOG_INF("%d ", len);
	// printk("ftp data:\n%.*s", len, (uint8_t *)msg);
	printk("ftp data:\n");
	printk(msg);
	// for(int i = 0; i < len; i += 128){//printk supports only 128bytes ata time
		// printk(msg+i);
	// }
}

void ftp_ctrl_callback(const uint8_t *msg, uint16_t len)
{
	// LOG_INF("%d ", len);
	// printk("ftp ctrl:\n%.*s", len, (uint8_t *)msg);
	printk("ftp ctrl:\n");
	printk(msg);
	// for(int i = 0; i < len; i += 128){//printk supports only 128bytes ata time
		// printk(msg+i);
	// }
	// printk("end of ctrl", len);
}

void main(void)
{
	char response[256];
	int ret;
		
	LOG_INF("begin!\n");
	
	/* raw disk i/o */
	do {
		static const char *disk_pdrv = "SD";
		uint64_t memory_size_mb;
		uint32_t block_count;
		uint32_t block_size;

		if (disk_access_init(disk_pdrv) != 0) {
			LOG_ERR("Storage init ERROR!");
			break;
		}

		if (disk_access_ioctl(disk_pdrv,
				DISK_IOCTL_GET_SECTOR_COUNT, &block_count)) {
			LOG_ERR("Unable to get sector count");
			break;
		}
		LOG_INF("Block count %u", block_count);

		if (disk_access_ioctl(disk_pdrv,
				DISK_IOCTL_GET_SECTOR_SIZE, &block_size)) {
			LOG_ERR("Unable to get sector size");
			break;
		}
		LOG_INF("Sector size %u\n", block_size);

		memory_size_mb = (uint64_t)block_count * block_size;
		LOG_INF("Memory Size(MB) %u\n", (uint32_t)memory_size_mb>>20);
	} while (0);

	mp.mnt_point = disk_mount_pt;

	int res = fs_mount(&mp);

	if (res == FR_OK) {
		LOG_INF("Disk mounted.\n");
		lsdir(disk_mount_pt);
	} else {
		LOG_ERR("Error mounting disk.\n");
	}
	
	sccb_setup();

	get_frame();


	LOG_INF("+++image end\n");

	struct fs_file_t imf;
	
	fs_file_t_init(&imf);
	LOG_DBG("zfp init\n");

	fs_open(&imf, "/SD:/im.bmp", FS_O_WRITE | FS_O_CREATE);
	LOG_DBG("zfp open\n");
	
	fs_write(&imf, bmp_header, BMPIMAGEOFFSET);
	fs_write(&imf, imbuf, IMAGE_SIZE_BYTES);
	LOG_DBG("zfp write\n");
	
	fs_close(&imf);
	LOG_DBG("zfp close\n");
	
	
		
	LOG_INF("AT modem begin\n");

	
	
	ret = nrf_modem_at_cmd(response, sizeof(response), "AT+CGMR");
	printk(response);
	
	ret = nrf_modem_at_printf("AT");
	if (ret) {LOG_ERR("AT failed\n");	return;	}
	LOG_INF("AT");
		
	
	
	ret = nrf_modem_at_cmd(response, sizeof(response), "AT+CGDCONT=0,\"IP\",\"hologram\"");
	printk(response);
	LOG_INF("CGDCONT");
	
	ret = nrf_modem_at_cmd(response, sizeof(response), "AT+CFUN=1");
	printk(response);
	LOG_INF("CFUN");
		
	ret = nrf_modem_at_cmd(response, sizeof(response), "AT+COPS=1,2,\"50501\"");
	printk(response);
	LOG_INF("COPS");
	
	ret = nrf_modem_at_cmd(response, sizeof(response), "AT+CEREG?");
	printk(response);
	
	ret = nrf_modem_at_cmd(response, sizeof(response), "AT+CPIN?");
	printk(response);
	
	
	ftp_init(ftp_ctrl_callback, ftp_data_callback);
	
	ret = ftp_open("ftp.bosl.com.au", 21, -1);
	
	ret = ftp_login("images@bosl.com.au", "solderflux");
		
	ret = ftp_type(FTP_TYPE_BINARY);

	ret = ftp_put("image.bmp", bmp_header, BMPIMAGEOFFSET, FTP_PUT_NORMAL);
	const uint16_t chuck_size = UINT16_MAX;
	for(uint32_t l = 0; l < IMAGE_SIZE_BYTES; l += chuck_size){
		uint16_t tsize = MIN(chuck_size, (IMAGE_SIZE_BYTES - l));
		ret = ftp_put("image.bmp", imbuf+l, tsize, FTP_PUT_APPEND);
	}
	
	LOG_INF("ftp put: %d", ret);
		
	ret = ftp_close();
		
	
	LOG_INF("UPLOAD SEQUENCE ENDED");
	
	
	while(1){
		k_msleep(100);
	}
	
	


}
