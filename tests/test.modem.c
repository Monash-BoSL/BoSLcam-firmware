
#include "ncs_version.h"
#if NCS_VERSION_NUMBER < 0x020100
    #include <zephyr.h>
#endif
#include <modem/lte_lc.h>
#include <nrf_modem.h>
#include <nrf_modem_at.h>
#include <modem/nrf_modem_lib.h>

#include "../src/common.h"
#include "../src/modem.h"

LOG_MODULE_REGISTER(test_modem);

int test_automatic_network_selection(void){
    LOG_INF("TEST: AUTOMATIC NETWORK SELECTION");

    nrf_modem_lib_init();
    int ret = nrf_modem_at_printf("AT");
    if(ret == 0){LOG_INF("AT initialised");}
    else if (ret < 0){LOG_ERR("AT initialisation error"); return ret;}


   struct ftp_config_t ftp_cfg = {
        .mccmnc = "50503",
        .apn = "simbase"
   };

    modem_network_register(&ftp_cfg);


    return 0;
}


int test_modem_shutdown_callback(void){
    int ret;
    LOG_INF("TEST: MODEM SHUTDOWN CALLBACK");

    nrf_modem_lib_init();

    ret = nrf_modem_at_printf("AT");
    if(ret == 0){LOG_INF("AT initialised");}
    else if (ret < 0){LOG_ERR("AT initialisation error"); return ret;}

    LOG_INF("lib shutdown");
    nrf_modem_lib_shutdown();

    nrf_modem_lib_init();

    ret = nrf_modem_at_printf("AT");
    if(ret == 0){LOG_INF("AT initialised");}
    else if (ret < 0){LOG_ERR("AT initialisation error"); return ret;}

    LOG_INF("wrapper shutdown");
    modem_shutdown();

    return 0;
}

int test_modem_shutdowns_trigger_reset(void){
    int ret;
    LOG_INF("TEST: MODEM SHUTDOWNS TRIGGER RESET");

    for(size_t i = 0;;i++){
        nrf_modem_lib_init();

        ret = nrf_modem_at_printf("AT");
        if(ret == 0){LOG_INF("AT initialised");}
        else if (ret < 0){LOG_ERR("AT initialisation error"); return ret;}

        LOG_INF("shutdown nos. %d", i);
        nrf_modem_lib_shutdown();
    }


    return 0;
}


int test_modem_psm(void){
    int ret;
    LOG_INF("TEST: MODEM PSM");

    nrf_modem_lib_init();

    ret = nrf_modem_at_printf("AT");
    if(ret == 0){LOG_INF("AT initialised");}
    else if (ret < 0){LOG_ERR("AT initialisation error"); return ret;}

   struct ftp_config_t ftp_cfg = {
        .mccmnc = "50501",
        .apn = "simbase"
   };

    ret = modem_network_register(&ftp_cfg);
    if(ret == 0){LOG_INF("REGISTERED ");}
    else if (ret < 0){LOG_ERR("REGISTRATION error"); return ret;}


    char buffer[1024];
    for(size_t i = 0;;i++){

        led(1);
        LOG_INF("wake nos. %d", i);
        modem_wait_registration(2000);

        ret = nrf_modem_at_cmd(buffer, sizeof(buffer), "AT+CEREG?");
        printk("%s", buffer);

        ret = nrf_modem_at_cmd(buffer, sizeof(buffer), "AT+CPSMS?");
        printk("%s", buffer);


        LOG_INF("sleep nos. %d", i);
        led(0);
        k_sleep(K_SECONDS(10));
    }


    return 0;

}

int test_parse_timer(void) {
    LOG_INF("TEST: parse_timer");

    int ret = 0;
    psm_timer_t t;

    parse_timer("00100001", &t);
    LOG_INF("00100001 -> unit=%u value=%u", t.unit, t.value);
    if (t.unit != 1 || t.value != 1) ret = -1;

    parse_timer("00000000", &t);
    LOG_INF("00000000 -> unit=%u value=%u", t.unit, t.value);
    if (t.unit != 0 || t.value != 0) ret = -1;

    parse_timer("11100000", &t);
    LOG_INF("11100000 -> unit=%u value=%u", t.unit, t.value);
    if (t.unit != 7 || t.value != 0) ret = -1;

    parse_timer("10111111", &t);
    LOG_INF("10111111 -> unit=%u value=%u", t.unit, t.value);
    if (t.unit != 5 || t.value != 31) ret = -1;

    parse_timer("01010101", &t);
    LOG_INF("01010101 -> unit=%u value=%u", t.unit, t.value);
    if (t.unit != 2 || t.value != 21) ret = -1;

    if (ret == 0) {
        LOG_INF("parse_timer: PASS");
    } else {
        LOG_ERR("parse_timer: FAIL");
    }

    return ret;
}

int test_modem_psm_mode(void) {
    int ret;
    uint8_t enabled;
    psm_timer_t tau;
    psm_timer_t active;

    LOG_INF("TEST: MODEM PSM");

    nrf_modem_lib_init();

    ret = nrf_modem_at_printf("AT");
    if (ret == 0) {
        LOG_INF("AT initialised");
    } else {
        LOG_ERR("AT initialisation error");
        return ret;
    }

    struct ftp_config_t ftp_cfg = {
        .mccmnc = "50501",
        .apn = "simbase"
    };

    ret = modem_network_register(&ftp_cfg);
    if (ret == 0) {
        LOG_INF("REGISTERED");
    } else {
        LOG_ERR("REGISTRATION error");
        return ret;
    }

    for (;;) {
        led(1);

        ret = modem_psm_mode(&enabled, &tau, &active);
        if (ret == 0) {
            LOG_INF("PSM enabled : %u", enabled);
            LOG_INF("TAU         : %u s", tau_to_seconds(&tau));
            LOG_INF("Active Time : %u s", active_time_to_seconds(&active));
        } else {
            LOG_ERR("Failed to read PSM configuration");
        }

        led(0);
        k_sleep(K_SECONDS(10));
    }

    return 0;
}

int test_tau_to_seconds(void) {
    LOG_INF("TEST: tau_to_seconds");

    int ret = 0;
    psm_timer_t t;
    int32_t out;

    t.unit = 0; t.value = 3;
    out = tau_to_seconds(&t);
    LOG_INF("10 min * 3 = %d", out);
    if (out != 1800) { ret = -1; }

    t.unit = 1; t.value = 2;
    out = tau_to_seconds(&t);
    LOG_INF("1 h * 2 = %d", out);
    if (out != 7200) { ret = -1; }

    t.unit = 2; t.value = 1;
    out = tau_to_seconds(&t);
    LOG_INF("10 h * 1 = %d", out);
    if (out != 36000) { ret = -1; }

    t.unit = 3; t.value = 10;
    out = tau_to_seconds(&t);
    LOG_INF("2 s * 10 = %d", out);
    if (out != 20) { ret = -1; }

    t.unit = 4; t.value = 4;
    out = tau_to_seconds(&t);
    LOG_INF("30 s * 4 = %d", out);
    if (out != 120) { ret = -1; }

    t.unit = 5; t.value = 3;
    out = tau_to_seconds(&t);
    LOG_INF("1 min * 3 = %d", out);
    if (out != 180) { ret = -1; }

    t.unit = 6; t.value = 1;
    out = tau_to_seconds(&t);
    LOG_INF("320 h * 1 = %d", out);
    if (out != 1152000) { ret = -1; }

    t.unit = 7; t.value = 10;
    out = tau_to_seconds(&t);
    LOG_INF("deactivated = %d", out);
    if (out != -1) { ret = -1; }

    if (ret == 0) {
        LOG_INF("tau_to_seconds: PASS");
    } else {
        LOG_ERR("tau_to_seconds: FAIL");
    }

    return ret;
}

int test_active_time_to_seconds(void) {
    LOG_INF("TEST: active_time_to_seconds");

    int ret = 0;
    psm_timer_t t;
    int32_t out;

    t.unit = 0; t.value = 5;
    out = active_time_to_seconds(&t);
    LOG_INF("2 s * 5 = %d", out);
    if (out != 10) { ret = -1; }

    t.unit = 1; t.value = 3;
    out = active_time_to_seconds(&t);
    LOG_INF("1 min * 3 = %d", out);
    if (out != 180) { ret = -1; }

    t.unit = 2; t.value = 2;
    out = active_time_to_seconds(&t);
    LOG_INF("6 min * 2 = %d", out);
    if (out != 720) { ret = -1; }

    t.unit = 3; t.value = 7;
    out = active_time_to_seconds(&t);
    LOG_INF("reserved unit = %d", out);
    if (out != -2) { ret = -1; }

    t.unit = 7; t.value = 9;
    out = active_time_to_seconds(&t);
    LOG_INF("deactivated = %d", out);
    if (out != -1) { ret = -1; }

    if (ret == 0) {
        LOG_INF("active_time_to_seconds: PASS");
    } else {
        LOG_ERR("active_time_to_seconds: FAIL");
    }

    return ret;
}