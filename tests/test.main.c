

#include <zephyr.h>

#include <modem/lte_lc.h>
#include <nrf_modem.h>
#include <nrf_modem_at.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(test_main);

int test_printf_uint64_t(void){
    int ret = 0;
    LOG_INF("TEST: PRINT uint64_t");

    uint64_t a = 1543;
    LOG_INF("%lu should be 1543", a);

    return 0;
}