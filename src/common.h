#pragma once

#include <date_time.h>

#define KEY 37
#define PW_SUFFIX "_Z4GQ3tjuzu"

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

#define BMPIMAGEOFFSET 66
#define IMAGE_WIDTH			(320)
#define IMAGE_HEIGHT		(240)
#define IMAGE_SIZE_BYTES 	(IMAGE_WIDTH*IMAGE_HEIGHT*2)

static const char bmp_header[BMPIMAGEOFFSET] =
{
  0x42, 0x4D, 0x36, 0x58, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00, 0x28, 0x00,
  0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00, 0x03, 0x00,
  0x00, 0x00, 0x00, 0x58, 0x02, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0xE0, 0x07, 0x00, 0x00, 0x1F, 0x00,
  0x00, 0x00
};


enum image_format {
    BMP = 0,
    JPG,
};

enum image_size {
    QVGA = 0,
    VGA,
    QQVGA,
};

struct image_config_t {
	uint32_t auto_range_time;
  enum image_format format;
  enum image_size   size;
	//image size 
	//awb enable
	//ae enable
	//...
};

enum cypher_type {
	NONE = 0,
	CAESAR,
	SUFFIX,
};


struct ftp_config_t {
	char* apn;
	char* network_operator;//numertic network operator code
	char* domain;
	//port
	char* username;
	enum cypher_type cyph_type;
	char* password;
	char* image_path;
	char* status_path;
};

struct sd_config_t {
	char* image_path;
	char* status_path;
	int	logging_level;//DBG, INF, WRN, ERR, OFF
};

enum trigger_type {
	TIME_TIRGGER = 0,
	UART_TRIGGER,
};

struct trigger_config_t {
	enum trigger_type trig_type;
	uint32_t logging_interval; //ms
	uint32_t logging_decimation_ftp;//1 in every x photos captured to sd will be uploaded
};

struct master_config_t {
	//struct gnss_config;
	struct trigger_config_t trig_cfg;
	struct image_config_t im_cfg;
	struct ftp_config_t ftp_cfg;
	struct sd_config_t sd_cfg;
};

struct capture_t {
	uint8_t* data;
	uint32_t length;
	int32_t time;
};

enum time_source {
	GNSS_TIME = 0,
	NETWORK_TIME,
	NTP_TIME,
	FS_TIME,
	NO_TIME,
	EXT_TIME,
};

static const char* time_source_str[] = {"GNSS_TIME",
								"NETWORK_TIME",
								"NTP_TIME",
								"FS_TIME",
								"NO_TIME",
								"EXT_TIME",};

struct status_t {
	int32_t system_time;
	int32_t battery_voltage;
	uint32_t captures;
	enum time_source time_src;
};


void LOG_UNIXTIME(const int32_t ln);
