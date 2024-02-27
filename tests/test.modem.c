

#include <zephyr.h>

#include <modem/lte_lc.h>
#include <nrf_modem.h>
#include <nrf_modem_at.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(test_modem);

int test_automatic_network_selection(void){
    int ret = 0;
    LOG_INF("TEST: AUTOMATIC NETWORK SELECTION");

    // int ret;
    // if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
    //     /* Do nothing, modem is already configured and LTE connected. */
    // } else {
    //     ret = lte_lc_init();
    //     if (ret) {
    //         printk("Modem initialization failed, error: %d\n", ret);
    //         return ret;
    //     }
    // }

    LOG_INF("modem init");


    ret = nrf_modem_at_printf("AT");
    if(ret == 0){LOG_INF("AT initialised");}
    else if (ret < 0){LOG_ERR("AT initialisation error"); return ret;}

    LOG_INF("AT_DONE");
    


    return 0;
}