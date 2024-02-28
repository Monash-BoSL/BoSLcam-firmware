
#include <zephyr.h>

#include <assert.h>
#include "test.modem.h"
#include "test.main.h"

int test_runtime(void){
    int ret = 0;
        
    // ret = test_automatic_network_selection();
    // assert(ret == 0);


    // ret = test_printf_uint64_t();
    // assert(ret == 0);
    test_sleepy();
    
    printf("All tests successful!");
    k_sleep(K_FOREVER);
    return 0;
}