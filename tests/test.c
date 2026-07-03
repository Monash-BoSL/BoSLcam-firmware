#include "ncs_version.h"
#if NCS_VERSION_NUMBER >= 0x020100
    #include <zephyr/kernel.h>
#else
    #include <zephyr.h>
#endif

#include <stdio.h>

#include <assert.h>
#include "test.modem.h"
#include "test.main.h"
#include "test.ov7675.h"

int test_runtime(void){
    // int ret = test_automatic_network_selection();
    // assert(ret == 0);

    // ret = test_printf_uint64_t();
    // assert(ret == 0);

    // test_sleepy();
    // test_led();

    // test_low_power();

    // test_modem_shutdown_callback();
    // test_modem_shutdowns_trigger_reset();
    test_modem_sms();


    printf("All tests successful!\n");
    k_sleep(K_FOREVER);
    return 0;
}
