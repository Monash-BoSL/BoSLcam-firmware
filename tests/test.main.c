
#include "ncs_version.h"
#if NCS_VERSION_NUMBER < 0x020100
    #include <zephyr.h>
#endif
#include <modem/lte_lc.h>
#include <nrf_modem.h>
#include <nrf_modem_at.h>
#include <hal/nrf_gpio.h>

#include "common.h"
#include "sd.h"

LOG_MODULE_REGISTER(test_main);

int test_printf_uint64_t(void){
    LOG_INF("TEST: PRINT uint64_t");

    uint64_t a = 1543;
    LOG_INF("%llu should be 1543", a);

    return 0;
}


int test_low_power(void){

    nrf_gpio_cfg_input(LED_FLASH_INBUILT_PIN, NRF_GPIO_PIN_PULLDOWN);//we haven't read the SD config file yet so we don't know which pin to pull down. We will guess the INBUILT one as it won't affect external UART if connected. This does mean that if the flash is external it will remain on until we read the config.
    NRF_UARTE0->ENABLE = 0;
    NRF_SPIM1->ENABLE = 0;
    NRF_TWIM2->ENABLE = 0;

    sdhc_mount();//very importaint for low power

    nrf_gpio_cfg_input(WKE_PIN, NRF_GPIO_PIN_PULLDOWN);
    nrf_gpio_cfg_input(SCCB_PDN, NRF_GPIO_PIN_PULLUP);

    while(1){
        LOG_INF("WAKING");
        LOG_INF("SLEEPING");
        k_msleep(10000);
    }
    return 0;
}
