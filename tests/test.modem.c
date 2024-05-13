

#include <modem/lte_lc.h>
#include <nrf_modem.h>
#include <nrf_modem_at.h>
#include <modem/nrf_modem_lib.h>

#include "../src/common.h"
#include "../src/ftp.h"

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