
#include <zephyr.h>

#include <assert.h>
#include "test.modem.h"

int test_runtime(void){
    int ret = 0;
        
    ret = test_automatic_network_selection();
    assert(ret == 0);

    printf("All tests successful!");
    k_sleep(K_FOREVER);
    return 0;
}