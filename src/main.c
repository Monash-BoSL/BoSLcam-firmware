
#include <nrfx.h>
#include <hal/nrf_gpio.h>

#include <inttypes.h>

#include <date_time.h>
#include <stdlib.h>


#include "common.h"
#include "ov7675.h"
#include "sd.h"
#include "modem.h"
#include "ftp.h"
#include "jpg.h"
#include "watchdog.h"


#ifdef CONFIG_DBG_TEST_RUNTIME
    #include "../tests/test.h"
#endif

/*************** VERSION NUMBER ********************/
#define _VERSION "v2.0.0rc"
// #define _VERSION "v2.0.0"
/*************** TODO *******************************
[X] check that time source remains correct when modem is reset
[X] add backup DNS configuration
[ ] we should store the status string when we write to the SDHC so that it the same on the SDHC and FTP
[ ] add 'damage' counter which will reset via WDT if too many errors accumulate
[ ] add logging of errors on SDHC and via upload
[X] log the signal quality
[X] perform an automatic search for networks when the default network is not found
    [ ] attempt an automatic connection if lots of uploads have failed
    [X] store a list of known networks with their signal quality
[ ] add automatic detection of when image is exposed well/remembering of last exposure settings
[ ] consider encoding image differences to better compression. Most objects in the static scene will not change with time.
[X] automatically make directories on sd card and ftp
    [X] automatically make 'images' folder on sd card.
[X] use yacc flex for parsing SD card config file
[ ] add versioning in config file
[ ] figure out how to name files when no network info
[X] add alarm based logging rather than delay based
    [X] logging based on length of last loop, not quite RTC yet though
[X] add jpeg mode
    [X] make jpeg compression work on VGA images.
    [X] does jpeg work now that the data in capture may not be the image data (investigate)
[X] add option to switch to 640x480
[ ] issue with network time reset on low power sleep?
[ ] find best idle states for pins (eg: 28) for low power
****************************************************/

LOG_MODULE_REGISTER(main);

struct capture_task_t {
    int64_t requested_at_ms;
    uint8_t upload;
    uint8_t respect_dark_noup;
};

#define CAPTURE_QUEUE_SIZE 5
K_MSGQ_DEFINE(capture_q, sizeof(struct capture_task_t), CAPTURE_QUEUE_SIZE, 1);

const struct device* gpio      = DEVICE_DT_GET(DT_NODELABEL(gpio0));
const struct device* i2c_sccb  = DEVICE_DT_GET(DT_NODELABEL(i2c2));
const struct device* spi_sram; /* init in a bit of a different way? */

uint8_t image_buffer[2*QVGA_WIDTH*QVGA_HEIGHT];

static struct master_config_t mcfg;
struct capture_t capture = {
                            .data = image_buffer, 
                            .capacity = sizeof(image_buffer), 
                            .size = 0, 

                            .aec = AEC_ON,
                            .agc = AGC_ON,
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
                                .network_searched = 0,
                                };


// This function puts a capture task into the capture queue. If the queue is full, it will drop the oldest item to make room for the new one.
void msgq_put_force(struct k_msgq *q, struct capture_task_t *task) {
    int ret_put = k_msgq_put(q, task, K_NO_WAIT);

    switch (ret_put) {
        case 0:
            LOG_INF("capture task pushed");
            break;
        case -ENOMSG:
        case -EAGAIN:
            LOG_WRN("capture queue full (ret=%d): dropping oldest capture task", ret_put);
            // Try to drop the oldest item to make room
            struct capture_t dummy;
            int ret_get = k_msgq_get(q, &dummy, K_NO_WAIT);
            if (ret_get == 0 || ret_get == -ENOMSG) {
                // Successfully dropped an item or queue was empty, retry the put
                msgq_put_force(q, task);
            } else {
                LOG_WRN("cannot empty capture queue (ret=%d): dropping this capture task", ret_get);
            }
            break;
        default:
            LOG_ERR("unexpected msgq error (ret=%d): cannot push wake trigger capture", ret_put);
            break;
        }
}

/******************************************************************************/
/* TIME TRIGGER                                                               */
/******************************************************************************/

void time_trigger_handler(void) {
    static int time_trigger_count = 0;
    const int64_t now_ms = k_uptime_get();

    const int d = mcfg.trig_cfg.logging_decimation_ftp; /* must exist as this thread only init after mcfg parsed successfully */

    const struct capture_task_t capture_task = {
        .requested_at_ms = now_ms,
        .upload = (d > 0) && (0 == (time_trigger_count % d)), /* respect logging decimation on time triggers */
        .respect_dark_noup = 1, /* respect dark_noup setting */
    };
    time_trigger_count++;

    msgq_put_force(&capture_q, &capture_task);
}

int init_time_trigger_capture(uint32_t interval_ms) {
    static struct k_timer capture_timer;
    k_timer_init(&capture_timer, time_trigger_handler, NULL);
    
    k_timer_start(&capture_timer, K_NO_WAIT, K_MSEC(interval_ms));
    
    LOG_INF("Successfully scheduled periodic capture timer for %u ms interval.", interval_ms);
    return 0;
}

/******************************************************************************/
/* WAKE TRIGGER                                                               */
/******************************************************************************/

void wake_trigger_handler(const struct device *dev,
              struct gpio_callback *cb,
              uint32_t pins) {
    static int64_t last_wake_trigger_ms = 0;
    const int64_t now_ms = k_uptime_get();

    if ((now_ms - last_wake_trigger_ms) < WAKE_PIN_TRIGGER_HOLDOUT_MS) { return; }
    last_wake_trigger_ms = now_ms;

    const struct capture_task_t capture_task = {
        .requested_at_ms = now_ms,
        .upload = 1,
        .respect_dark_noup = 0, /* wake trigger should not respect the dark noup */
    };

    msgq_put_force(&capture_q, &capture_task);
}

static struct gpio_callback gpio_cb;
int init_wake_trigger_capture(void){
    int ret = 0;

    ret = gpio_pin_configure(gpio, WKE_PIN, GPIO_INPUT | GPIO_PULL_DOWN);
    if (ret) {
        LOG_ERR("Failed to configure WKE pin, error: %d", ret);
        return ret;
    }

    ret = gpio_pin_interrupt_configure(gpio, WKE_PIN, GPIO_INT_EDGE_RISING);
    if (ret) {
        LOG_ERR("Failed to configure WKE interrupt, error: %d", ret);
        return ret;
    }

    gpio_init_callback(&gpio_cb, wake_trigger_handler, BIT(WKE_PIN));

    ret = gpio_add_callback(gpio, &gpio_cb);
    if (ret) {
        LOG_ERR("Failed to add wke interrupt callback, error: %d", ret);
        return ret;
    }else{
        LOG_INF("Enabled wke interrupt success!");
    }

    return 0;
}

/******************************************************************************/
/* CAPTURE WORKER                                                             */
/******************************************************************************/

int capture_f(const struct capture_task_t* const capture_task){
    int ret;
    watchdog_feed(); // TODO: BUG: this will fail if the capture is done less than once a day

    uint8_t mean_rgb;

    get_time(&capture.time);
    update_status();

    LOG_INF("ov7675 initialisation");
    ov7675_init(&mcfg.im_cfg, &capture);

    LOG_INF("ov7675 capture");
    int do_mean = (capture_task->respect_dark_noup) 
                  && (mcfg.trig_cfg.dark_noup != 0xFF) 
                  && (mcfg.trig_cfg.dark_noup != 0);

    ov7675_capture_sdhc_buffered(mcfg.im_cfg.flash, &capture, do_mean, &mean_rgb);
    LOG_INF("mean_rgb: %u", mean_rgb);

    LOG_INF("ov7675 deinit");
    ov7675_deinit(mcfg.im_cfg.flash);

#ifdef CONFIG_DBG_SEND_IMAGE_RTT
    sdhc_file_to_rtt(SCRATCH_FILE);
#endif

    LOG_INF("image -> sdhc");
    ret = sdhc_move_image(mcfg.sd_cfg.image_path, &capture);
    if(ret < 0){LOG_ERR("sdhc_move_image fail! ret=%d",ret);}

    LOG_INF("status -> sdhc");
    ret = sdhc_write_status(mcfg.sd_cfg.status_path, &stats_global);
    if(ret < 0){LOG_ERR("sdhc_write_status fail! ret=%d",ret);}

    if(mcfg.im_cfg.format == JPG){
        LOG_INF("jpg -> sdhc");
        ret = sdhc_write_jpg(mcfg.sd_cfg.image_path, &capture);
        if(ret < 0){LOG_ERR("sdhc_write_jpg fail! ret=%d", ret);}
    }

    // check that this gracefully exits if the signal is low and continues with SD only logging for this loop
    if (capture_task->upload){
        if (!do_mean || ! (mean_rgb < mcfg.trig_cfg.dark_noup)){ /* if the image is not dark */
            LOG_INF("image -> ftp");

            ret = ftp_write_image(&mcfg.ftp_cfg, &capture);
            if(ret){modem_network_deregister();}
        }

        LOG_INF("status -> ftp");
        ret = ftp_write_status(&mcfg.ftp_cfg, &stats_global);
        if(ret){modem_network_deregister();}
    }

    LOG_INF("done");

    k_msleep(1000); //guarantee other threads time to execute
    return 0;
}

void capture_worker_f(void *p1, void *p2, void *p3){
    /*
     * this thread should only start processing once setup() has passed
     * in practice this guaranteed by only submitting tasks to the queue once
     * setup() passes.
     */
    struct capture_task_t capture_task;

    while(1){
        if (!k_msgq_get(&capture_q, &capture_task, K_FOREVER)){ // always 0 for K_FOREVER unless queue is purged
            int ret_cap = capture_f(&capture_task);
            stats_global.captures++;
            // TODO: what to do with ret?
        }
    }
}

/* setup the worker thread */
#define CAPTURE_WORKER_THREAD_STACK_SIZE (4096)
K_THREAD_DEFINE(capture_worker, CAPTURE_WORKER_THREAD_STACK_SIZE, capture_worker_f, NULL, NULL, NULL,
        CONFIG_MAIN_THREAD_PRIORITY, K_ESSENTIAL, 0);

/******************************************************************************/
/* SETUP                                                                      */
/******************************************************************************/

int get_time(int32_t* ct){
    int ret = 0;

    int64_t unix_time_ms;
    ret = date_time_now(&unix_time_ms);
    if(ret < 0){return ret;}
    *ct = (uint32_t) (unix_time_ms/1000);

    return 0;
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

int setup(void){
    int ret = 0;

    ret = watchdog_init_and_start();

    ret = sdhc_mount();//very importaint for low power

    ret = sdhc_load_config(CONFIG_FILE, &mcfg);
    if(ret < 0){
        LOG_ERR("failed to load config. halting");
        return ret;
    }
#ifdef CONFIG_DBG_CONFIG_OVERLAY
    _dbg_config_overlay(&mcfg);
#endif

    nrf_gpio_cfg_input(mcfg.im_cfg.flash, NRF_GPIO_PIN_PULLDOWN);

    int d = mcfg.trig_cfg.logging_decimation_ftp;
    if(d > 0){//first try get time from network
        ftp_setup();

        modem_init();

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
        LOG_WRN("no network time, resorting to SD");
        struct tm cal;
        ret = sdhc_load_last_status_time(mcfg.sd_cfg.status_path, &cal);
        if(ret == 0){
            ret = date_time_set(&cal);
            stats_global.time_src = FS_TIME;
        }
    }
    if(!date_time_is_valid()){//then from default time epoch
        LOG_WRN("no valid time, resorting to default");
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

int main(void){
    int ret = 0;

    // for the test suit to work it should always remain here as the first line of code!
#ifdef CONFIG_DBG_TEST_RUNTIME
    printk("*** ENTERING TEST RUNTIME ***");
    test_runtime();
#endif

    printk("*** BoSLcam firmware " _VERSION " complied on " __DATE__ " at " __TIME__ " ***\n");

    //some low power stuff

    //try out high and low for min power
    // nrf_gpio_cfg_input( 28, NRF_GPIO_PIN_PULLUP);
    //
    nrf_gpio_cfg_input(LED_FLASH_INBUILT_PIN, NRF_GPIO_PIN_PULLDOWN);//we haven't read the SD config file yet so we don't know which pin to pull down. We will guess the INBUILT one as it won't affect external UART if connected. This does mean that if the flash is external it will remain on until we read the config.
    NRF_UARTE0->ENABLE = 0;
    NRF_SPIM1->ENABLE = 0;
    NRF_TWIM2->ENABLE = 0;


    /* useful for knowing application has launched in the field */
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


    /* init capture triggers */
    ret = init_time_trigger_capture(mcfg.trig_cfg.logging_interval);
    ret = init_wake_trigger_capture(); /* failure not fatal */

   while(1){
        k_sleep(K_FOREVER); /* execution now thread based */
    } 

    /* this is dead code because all the execution threads are essential but this below is the correct graceful shutdown code */
    nrf_gpio_cfg_input(LED_FLASH_INBUILT_PIN, NRF_GPIO_PIN_PULLDOWN);//we haven't read the SD config file yet so we don't know which pin to pull down. We will guess the INBUILT one as it won't affect external UART if connected. This does mean that if the flash is external it will remain on until we read the config.
    nrf_gpio_cfg_input(WKE_PIN, NRF_GPIO_PIN_PULLDOWN);
    nrf_gpio_cfg_input(SCCB_PDN, NRF_GPIO_PIN_PULLUP);

    return 0;       // suppress compiler warning about main() having to return int
}

#ifdef CONFIG_DBG_CONFIG_OVERLAY
void _dbg_config_overlay(struct master_config_t* mcfg){
    mcfg->im_cfg.format = BMP;
    mcfg->im_cfg.resolution = QVGA;
    mcfg->im_cfg.use_flash = 0;
    mcfg->trig_cfg.logging_interval = 240000;
    mcfg->trig_cfg.logging_decimation_ftp = 0;
    mcfg->trig_cfg.dark_noup = 30;
}
#endif

