

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


int tsleepy(uint32_t target_duration_ms){
    int ret = 0;

    static int64_t unix_time_ms_last_call = 0;
    int64_t unix_time_ms_now = k_uptime_get();
    int64_t unix_time_ms_elapsed = unix_time_ms_now - unix_time_ms_last_call;
    LOG_INF("log_inf: %ld", unix_time_ms_elapsed);

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



int test_sleepy(void){
    while(1){
        LOG_INF("WAKING");
        k_msleep(2349);
        LOG_INF("SLEEPING");
        tsleepy(15000);
    }
    return 0;
}