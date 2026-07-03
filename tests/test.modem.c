
#include "ncs_version.h"
#if NCS_VERSION_NUMBER < 0x020100
    #include <zephyr.h>
#endif
#include <modem/lte_lc.h>
#include <nrf_modem.h>
#include <nrf_modem_at.h>
#include <modem/nrf_modem_lib.h>
#include <modem/sms.h>

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

// sms_callback_t
void sms_callback(struct sms_data* const data, void* context){
    LOG_INF("TYPE, %d", data->type);
    LOG_INF("HEADER...");
    // LOG_INF("    TIME, %s", data->header.deliver.time);
    // LOG_INF("    ADDRESS, %s", data->header.deliver.originating_address);
    // LOG_INF("    PORT, %s", data->header.deliver.app_port);
    // LOG_INF("    CONCAT, %s", data->header.deliver.concatenated);
    LOG_INF("PAYLOAD_LEN, %d", data->payload_len);
    LOG_INF("PAYLOAD, %s", data->payload);

}

int test_modem_sms(void){
    int ret;
    LOG_INF("TEST: MODEM SMS");

    nrf_modem_lib_init();

    ret = nrf_modem_at_printf("AT");
    if(ret == 0){LOG_INF("AT initialised");}
    else if (ret < 0){LOG_ERR("AT initialisation error"); return ret;}

    ret = sms_register_listener(sms_callback, NULL);
    if(ret == 0){LOG_INF("SMS CALLBACK INIT");}
    else if (ret < 0){LOG_ERR("SMS CALLBACK ERROR: %d", ret); return ret;}

    const char* sms = "abc";
    const char* number = "61468338675";

    ret = sms_send_text(number, sms);
    if(ret == 0){LOG_INF("SMS SEND SUCCESS");}
    else if (ret < 0){LOG_ERR("SMS SEND ERROR: %d", ret); return ret;}

    for ( ;; ){
        k_sleep(K_FOREVER);
    }
    return ret;
}