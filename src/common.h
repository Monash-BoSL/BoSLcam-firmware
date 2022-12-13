#pragma once

#include <time.h>

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


struct image_config_t {
	uint32_t auto_range_time;
	//image size 
	//awb enable
	//ae enable
	//...
};

struct ftp_config_t {
	char* apn;
	char* network_operator;//numertic network operator code
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

struct status_t {
	int32_t system_time;
	int32_t battery_voltage;
	uint32_t captures;
};


static void unix_date(struct tm* cal, int32_t unixtime){
  uint32_t seconds, minutes, hours, days, year, month;
  uint32_t day_of_week;
  seconds = unixtime;
  
  minutes  = seconds / 60;
  hours    = minutes / 60;
  days     = hours   / 24;

  seconds = seconds % 60;
  minutes = minutes % 60;
  hours   = hours % 24;

  /* Unix time starts in 1970 on a Thursday */
  year      = 1970;
  day_of_week = 4;

  while(1)
  {
    bool     leap_year   = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    uint16_t days_in_year = leap_year ? 366 : 365;
    if (days >= days_in_year)
    {
      day_of_week += leap_year ? 2 : 1;
      days      -= days_in_year;
      if (day_of_week >= 7)
        day_of_week -= 7;
      ++year;
    }
    else
    {
      cal->tm_yday = days;
      day_of_week  += days;
      day_of_week  %= 7;

      /* calculate the month and day */
      static const uint8_t days_in_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
      for(month = 0; month < 12; ++month)
      {
        uint8_t dim = days_in_month[month];

        /* add a day to feburary if this is a leap year */
        if (month == 1 && leap_year)
          ++dim;

        if (days >= dim)
          days -= dim;
        else
          break;
      }
      break;
    }
  }

  cal->tm_sec  = seconds;
  cal->tm_min  = minutes;
  cal->tm_hour = hours;
  cal->tm_mday = days + 1;
  cal->tm_mon  = month;
  cal->tm_year = year-1900;//for correct callender epoch
  cal->tm_wday = day_of_week;
}