

#include <zephyr.h>
#include <drivers/gpio.h>


#include "../src/common.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(test_ov7675);


int test_led(void){
    int ret = 0;

    const struct device* gpio;


    gpio = device_get_binding(DT_LABEL(DT_NODELABEL(gpio0)));
    LOG_INF("bind %s\n", gpio->name);
    gpio_pin_configure(gpio, TX_LED_PIN, GPIO_OUTPUT);



    while(1){

        gpio_pin_set_raw(gpio, TX_LED_PIN, 1);
        k_msleep(500);

        gpio_pin_set_raw(gpio, TX_LED_PIN, 0);
        k_msleep(500);

    }

    return 0;
}

