#pragma once

#include <date_time.h>

// #define _DBG_SEND_IMAGE_RTT //enable to switch logic out for sending image over RTT after taken

#define WATCHDOG_TIMEOUT_SEC 86400 //allows silly values

#define SUFFIX_KEY "_Z4GQ3tjuzu"
#define CAESAR_KEY  (37)

#define CAMADDR_WR  (0x42)
#define CAMADDR_RD  (0x43)
#define CAMADDR		(0x21)

#define SCCB_VS		(11)
#define SCCB_HREF	(12)
#define SCCB_PCLK	(13)
#define SCCB_XCLK	(14)
#define SCCB_PEN	(15)
#define SCCB_PDN	(16)

#define DBGPIN		(18)

#define TX_PIN                  (19)

#define LED_FLASH_EXTERNAL_PIN      (TX_PIN)
#define LED_FLASH_INBUILT_PIN       (17)

#define SCCB_CLK_DPPI_CH 	(0)
#define GPIOTE_CLK_TSK 		(0)

#define MAX_PATH (256)

#define RBG565_PIXEL_SIZE_BYTES 	(2)

#define BMPIMAGEOFFSET 		(66)

#define VGA_WIDTH 		(640)
#define VGA_HEIGHT 		(480)
#define CIF_WIDTH 		(352)
#define CIF_HEIGHT 		(240)
#define QVGA_WIDTH 		(320)
#define QVGA_HEIGHT 	(240)
#define QCIF_WIDTH 		(176)
#define QCIF_HEIGHT 	(144)
#define QQVGA_WIDTH 	(160)
#define QQVGA_HEIGHT 	(120)


#define DISK_MOUNT_PT "/SD:"
#define SCRATCH_FILE  "/scratch.bmp"
#define CONFIG_FILE   "/config.txt"
#define YY_PARSE_BUFFER_SIZE       (4096)

#define SDHC_PATH(strconst) DISK_MOUNT_PT strconst
#define STRLEN(strconst) (sizeof(strconst)-1)

enum image_format {
    BMP = 0,
    JPG,
};

enum image_resolution {
    VGA = 0,
    QVGA,
    QQVGA,
};

enum aec_t {
    AEC_OFF = 0,
    AEC_ON,
};

enum agc_t {
    AGC_OFF = 0,
    AGC_ON,
};

//read gain as 1.<mantissa> x 2^<exponent>
//mantissa is 4 bits.
//exponent is between 0 - 6.
struct gain_t {
    uint8_t mantissa;
    uint8_t exponent;
};

struct image_resolution_properties {
    uint16_t width;
    uint16_t height;
    char* bmp_header;
};

enum flash_t {
    NO_FLASH = 0xFF,
    LED_INBUILT_FLASH = LED_FLASH_INBUILT_PIN,
    LED_EXTERNAL_FLASH = LED_FLASH_EXTERNAL_PIN,
};

struct image_config_t {
    uint32_t                auto_range_time;
    enum image_format       format;
    enum image_resolution   resolution;
    enum flash_t            flash;
    uint32_t                use_flash;
    enum aec_t              aec;
    uint16_t                exposure;
    enum agc_t              agc;
    struct gain_t    gain;
};

enum cypher_t {
    NONE = 0,
    CAESAR,
    SUFFIX,
};


struct ftp_config_t {
    char* apn;
    char* mccmnc;//numertic network operator code
    char* domain;
    //port
    char* username;
    enum cypher_t cypher;
    char* password;
    char* image_path;
    char* status_path;
};

struct sd_config_t {
    char* image_path;
    char* status_path;
    uint32_t logging_level;//DBG, INF, WRN, ERR, OFF
};

enum trigger_t {
    TIME_TRIGGER = 0,
    UART_TRIGGER,
};

struct trigger_config_t {
    enum trigger_t trigger;
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

enum data_location { SRAM = 0, DISK };
struct capture_t {
    enum data_location where;
    char fp[MAX_PATH];
    uint8_t* const data;
    const size_t capacity;
    size_t size;

    enum aec_t aec;
    uint16_t exposure;
    enum agc_t agc;
    struct gain_t gain;
    enum image_resolution resolution;
    enum image_format format;

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

static const char* time_source_str[] = {
                                        "GNSS_TIME",
                                        "NETWORK_TIME",
                                        "NTP_TIME",
                                        "FS_TIME",
                                        "NO_TIME",
                                        "EXT_TIME",
                                        };


struct status_t {
    int32_t system_time;
    int32_t battery_voltage;
    uint32_t captures;
    enum time_source time_src;
    char mccmnc[7];
    uint8_t rsrq;
    uint8_t rsrp;
    uint8_t network_searched; 
};


static const char bmp_header_vga[BMPIMAGEOFFSET] =
{
  0x42, 0x4D, 0x36, 0x58, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00,
  0x00, 0x28, 0x00, 0x00, 0x00,
  0x80, 0x02, 0x00, 0x00, //pixel width (little endian)		[640]
  0xE0, 0x01, 0x00, 0x00, //pixel height (little endian)	[480]
  0x01, 0x00, 0x10, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x58, 0x02, 0x00, 0xC4,
  0x0E, 0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0xE0, 0x07, 0x00, 0x00, 0x1F, 0x00, 0x00,
  0x00
};

static const char bmp_header_qvga[BMPIMAGEOFFSET] =
{
  0x42, 0x4D, 0x36, 0x58, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00,
  0x00, 0x28, 0x00, 0x00, 0x00,
  0x40, 0x01, 0x00, 0x00, //pixel width (little endian)  [320]
  0xF0, 0x00, 0x00, 0x00, //pixel height (little endian) [240]
  0x01, 0x00, 0x10, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x58, 0x02, 0x00, 0xC4,
  0x0E, 0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0xE0, 0x07, 0x00, 0x00, 0x1F, 0x00, 0x00,
  0x00
};

static const struct image_resolution_properties image_resolutions[] = {
                                /*VGA*/
                                {
                                    .width = VGA_WIDTH,
                                    .height = VGA_HEIGHT,
                                    .bmp_header = (char*) bmp_header_vga,
                                },
                                /*QVGA*/
                                {
                                    .width = QVGA_WIDTH,
                                    .height = QVGA_HEIGHT,
                                    .bmp_header = (char*) bmp_header_qvga,
                                },
                                /*QQVGA*/
                                {
                                    .width = QQVGA_WIDTH,
                                    .height = QQVGA_HEIGHT,
                                    .bmp_header = NULL,
                                },
                            };

int LOG_UNIXTIME(const int32_t ln);

void led(bool on);
