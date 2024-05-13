#include "../src/common.h"

LOG_MODULE_REGISTER(test_ov7675);


int test_led(void){
    const struct device* gpio;


    gpio = device_get_binding(DT_LABEL(DT_NODELABEL(gpio0)));
    LOG_INF("bind %s\n", gpio->name);
    gpio_pin_configure(gpio, LED_FLASH_EXTERNAL_PIN, GPIO_OUTPUT);



    while(1){

        gpio_pin_set_raw(gpio, LED_FLASH_EXTERNAL_PIN, 1);
        k_msleep(500);

        gpio_pin_set_raw(gpio, LED_FLASH_EXTERNAL_PIN, 0);
        k_msleep(500);

    }

    return 0;
}

