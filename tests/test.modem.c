
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
    int tau;
    int active;
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

    lte_lc_psm_req(0);

    char buffer[1024];
    for(size_t i = 0;;i++){

        led(1);
        LOG_INF("wake nos. %d", i);
        modem_wait_registration(2000);

        ret = nrf_modem_at_cmd(buffer, sizeof(buffer), "AT+CEREG?");
        printk("%s", buffer);

        ret = nrf_modem_at_cmd(buffer, sizeof(buffer), "AT+CPSMS?");
        printk("%s", buffer);


        ret = lte_lc_psm_get(&tau, &active);

        if (ret == 0) { LOG_INF("PSM OK: tau=%d sec, active=%d sec", tau, active); } 
        else { LOG_ERR("PSM FAIL: ret=%d", ret); }

        LOG_INF("sleep nos. %d", i);
        led(0);
        k_sleep(K_SECONDS(10));
    }


    return 0;

}