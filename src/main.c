
#include <nrfx.h>
#include <nrf_modem_at.h>
#include <hal/nrf_gpio.h>

#include <zephyr.h>
#include <device.h>

#include <sys/util.h>
#include <sys/printk.h>
#include <inttypes.h>
#include <logging/log.h>

#include <date_time.h>

#include <modem/lte_lc.h>
#include <nrf_socket.h>

#include "common.h"
#include "ov7675.h"
#include "sd.h"
#include "ftp.h"
#include "jpg.h"
#include "watchdog.h"

#include "../tests/test.h"

/*************** TODO *******************************
[X] add backup DNS configuration
[ ] we should store the status string when we write to the SDHC so that it the same on the SDHC and FTP
[X] log the signal quality
[X] perform an automatic search for networks when the default network is not found
    [ ] attempt an automatic connection if lots of uploads have failed
    [X] store a list of known networks with their signal quality
[ ] add automatic detection of when image is exposed well/remembering of last exposure settings
[ ] consider encoding image differences to better compression. Most objects in the static scene will not change with time.
[X] automatically make directories on sd card and ftp
    [X] automatically make 'images' folder on sd card. 
[ ] use yacc flex for parsing SD card config file
[ ] add versioning in config file
[ ] figure out how to name files when no network info
[ ] add alarm based logging rather than delay based
    [X] logging based on length of last loop, not quite RTC yet though
[X] add jpeg mode
    [X] make jpeg compression work on VGA images.
    [X] does jpeg work now that the data in capture may not be the image data (investigate)
[X] add option to switch to 640x480
[ ] issue with network time reset on low power sleep?
[ ] find best idle states for pins (eg: 28) for low power
****************************************************/


LOG_MODULE_REGISTER(main);

const struct device * gpio;
const struct device * i2c_sccb;
const struct device * spi_sram;

uint8_t image_buffer[2*QVGA_WIDTH*QVGA_HEIGHT];

static struct master_config_t mcfg;
struct capture_t capture = {
                            .data = image_buffer, 
                            .capacity = sizeof(image_buffer), 
                            .size = 0, 
                            .resolution = QVGA, 
                            .format = BMP, 
                            .time = 0
                            };
struct status_t stats_global = {
                                .system_time = 0, 
                                .battery_voltage = -1, 
                                .captures = 0, 
                                .mccmnc = "\0\0\0\0\0\0\0", 
                                .rsrq = 0xFF, 
                                .rsrp = 0xFF,
                                .network_searched = 0
                                };

int sleepy(uint32_t target_duration_ms){
    int ret = 0;

    nrf_gpio_cfg_input( SCCB_PEN, NRF_GPIO_PIN_PULLDOWN);
    nrf_gpio_cfg_input( SCCB_PDN, NRF_GPIO_PIN_PULLUP);

    static int64_t unix_time_ms_last_call = 0;
    int64_t unix_time_ms_now = k_uptime_get();
    int64_t unix_time_ms_elapsed = unix_time_ms_now - unix_time_ms_last_call;

    if (unix_time_ms_elapsed < target_duration_ms) {
        int64_t sleep_ms = target_duration_ms - unix_time_ms_elapsed;
        if(sleep_ms > target_duration_ms){
            LOG_ERR("bad last sleep time, defaulting to %ld ms sleep", target_duration_ms);
            k_msleep(target_duration_ms);
            ret = -4; goto cleanup;
        }
        LOG_INF("Sleeping for: %ld ms", sleep_ms);
        ret = k_msleep(sleep_ms); goto cleanup;
    } else {
        LOG_WRN("Loop duration too long, continuing without sleep");
        ret = -1; goto cleanup;
    }

    ret = -3; goto cleanup;
cleanup:
    unix_time_ms_last_call = k_uptime_get();
    return ret;
}

int get_time(int32_t* ct){
    int ret = 0;

    int64_t unix_time_ms;
    ret = date_time_now(&unix_time_ms);
    if(ret < 0){return ret;}
    *ct = (uint32_t) (unix_time_ms/1000);

    return 0;
}

int slm_vbat(int* bat_mv){
    int ret = 0;
    char response[1024];

    ret = nrf_modem_at_cmd(response, sizeof(response), "AT%%XVBAT");
    if(ret == 0){
        char* start = strchr(response, ':')+1;
        char* end = strchr(start, '\n');
        *bat_mv = strtol(start, &end, 10);
    }
    return ret;
}


int update_status(){
    stats_global.system_time = capture.time;
    slm_vbat(&stats_global.battery_voltage);

    return 0;
}


void time_source_stats_async(const struct date_time_evt* evt){
    switch (evt->type){
    case DATE_TIME_OBTAINED_MODEM:
        stats_global.time_src = NETWORK_TIME;
        break;
    case DATE_TIME_OBTAINED_NTP:
        stats_global.time_src = NTP_TIME;
        break;
    case DATE_TIME_OBTAINED_EXT:
        stats_global.time_src = EXT_TIME;
        break;
    case DATE_TIME_NOT_OBTAINED:
        stats_global.time_src = NO_TIME;
        break;
    }
}

int modem_init(void){
    int ret = 0;

    if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
        /* Do nothing, modem is already configured and LTE connected. */
    } else {
        ret = lte_lc_init();
        if (ret) {
            printk("Modem initialization failed, error: %d\n", ret);
            return ret;
        }
    }

	struct nrf_in_addr dns;
	dns.s_addr = 0x08080808; // Google DNS, 8.8.8.8
	ret = nrf_setdnsaddr(NRF_AF_INET, &dns, sizeof(struct nrf_in_addr));

    return ret;
}


int configure_low_power(void){
    int err;

    /** Power Saving Mode */
    err = lte_lc_psm_req(true);
    if (err) {
        printk("lte_lc_psm_req, error: %d\n", err);
    }

    /** enhanced Discontinuous Reception */
    err = lte_lc_edrx_req(true);
    if (err) {
        printk("lte_lc_edrx_req, error: %d\n", err);
    }

    // /** Release Assistance Indication  */
    // err = lte_lc_rai_req(true);
    // if (err) {
        // printk("lte_lc_rai_req, error: %d\n", err);
    // }


    return err;
}

int setup(void){
    int ret = 0;

    ret = watchdog_init_and_start();

    ret = sdhc_mount();//very importaint for low power

    ret = sdhc_load_config("/config.txt", &mcfg);
    if(ret < 0){
        LOG_ERR("failed to load config. halting");
        return ret;
    }

    int d = mcfg.trig_cfg.logging_decimation_ftp;
    if(d > 0){//first try get time from network
        ftp_setup();

        modem_init();
        configure_low_power();//this does not seem to work at the moment 

        //gets current network time
        ret = modem_network_register(&mcfg.ftp_cfg);
        if(ret == 0){
            date_time_update_async(NULL);
            LOG_INF("busy wait for valid time");
            for(uint32_t i = 0; i < 1000; i++){
                if(date_time_is_valid()){break;}
                k_msleep(10);
            }
            stats_global.time_src = NETWORK_TIME;
        }
    }

    date_time_register_handler(time_source_stats_async);

    if(!date_time_is_valid()){//then from SD
        LOG_ERR("no network time, resorting to SD");
        struct tm cal;
        ret = sdhc_load_last_status_time(mcfg.sd_cfg.status_path, &cal);
        if(ret == 0){
            ret = date_time_set(&cal);
            stats_global.time_src = FS_TIME;
        }
    }
    if(!date_time_is_valid()){//then from default time epoch
        LOG_ERR("no valid time, resorting to default");
        struct tm cal = {	.tm_sec = 0,
                            .tm_min = 0,
                            .tm_hour = 0,
                            .tm_mday = 1,
                            .tm_mon = 0,
                            .tm_year = 120,
                            .tm_wday = 0,
                            .tm_yday = 0,
                            .tm_isdst = 0,
                        };//2020/01/01-00:00:00 UTC
        ret = date_time_set(&cal);
        stats_global.time_src = NO_TIME;
    }
    if(!date_time_is_valid()){return -ENODATA;}


    return 0;
}

int loop(void){
    int ret;
    watchdog_feed();

    int d = mcfg.trig_cfg.logging_decimation_ftp;

    get_time(&capture.time);
    update_status();

    LOG_INF("ov7675 initialisation");
    ov7675_init(mcfg.im_cfg.auto_range_time, mcfg.im_cfg.resolution, mcfg.im_cfg.format, &capture);

    LOG_INF("ov7675 capture");
    ov7675_capture_sdhc_buffered(&capture);

#ifdef _DBG_SEND_IMAGE_RTT
    sdhc_file_to_rtt(SCRATCH_FILE);
#endif

    LOG_INF("image -> sdhc");
    ret = sdhc_move_image(mcfg.sd_cfg.image_path, &capture);
    if(ret < 0){LOG_ERR("sdhc_move_image fail! ret=%d",ret);}

    LOG_INF("status -> sdhc");
    ret = sdhc_write_status(mcfg.sd_cfg.status_path, &stats_global);
    if(ret < 0){LOG_ERR("sdhc_write_status fail! ret=%d",ret);}

    if(mcfg.im_cfg.format == JPG){
        LOG_INF("jpg   -> sdhc");
        ret = sdhc_write_jpg(mcfg.sd_cfg.image_path, &capture);
        if(ret < 0){LOG_ERR("sdhc_write_jpg fail! ret=%d", ret);}
    }

    if ((d > 0) && (0 == (stats_global.captures % d))){
        LOG_INF("image -> ftp");

        ret = ftp_write_image(&mcfg.ftp_cfg, &capture);
        if(ret){modem_network_deregister();}

        LOG_INF("status -> ftp");
        ret = ftp_write_status(&mcfg.ftp_cfg, &stats_global);
        if(ret){modem_network_deregister();}
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

    stats_global.captures++;

    return 0;
}


void main(void){
    int ret = 0;

    // for the test suit to work it should always remain here as the first line of code!
    // if(true){test_runtime();};

    //some low power stuff

    //try out high and low for min power
    // nrf_gpio_cfg_input( 28, NRF_GPIO_PIN_PULLUP);
    //
    NRF_UARTE0->ENABLE = 0;
    NRF_SPIM1->ENABLE = 0;
    NRF_TWIM2->ENABLE = 0;

    LOG_INF("begin!");

    led(1);
    k_msleep(1000);
    led(0);

    ret = setup();
    if(ret < 0){
        LOG_ERR("setup failed with result %d!", ret);
        k_oops();
        //lockup program and halt
        //try call for help
    }

    while(1){
        loop();
    }

    nrf_gpio_cfg_input(SCCB_PEN, NRF_GPIO_PIN_PULLDOWN);
    nrf_gpio_cfg_input(SCCB_PDN, NRF_GPIO_PIN_PULLUP);


    while(1){
        k_msleep(1000);
    }
}
