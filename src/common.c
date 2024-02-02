#include "common.h"

#include <drivers/led.h>


void LOG_UNIXTIME(const int32_t ln){
    int ret = 0;
    int32_t ct;
    uint64_t unix_time_ms;
    ret = date_time_now(&unix_time_ms);
    if(ret < 0){return ret;}
    ct = (uint32_t) (unix_time_ms/1000);

    printk("%d: UNIX TIME: %d s\n", ln, ct);


    return 0;
}

#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>


#define LED0_NODE DT_ALIAS(led0)

#define LED0	DT_GPIO_LABEL(LED0_NODE, gpios)
#define PIN	DT_GPIO_PIN(LED0_NODE, gpios)
#define FLAGS	DT_GPIO_FLAGS(LED0_NODE, gpios)

const struct device *leddev;

void led(bool on) {
    int ret = 0;

    leddev = device_get_binding(LED0);

    ret = gpio_pin_configure(leddev, PIN, GPIO_OUTPUT_ACTIVE | FLAGS);
    if (ret < 0) {
        return;
    }

    gpio_pin_set(leddev, PIN, (int)on);
}
