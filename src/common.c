#include "common.h"



static const char* time_source_str[] = {
                                        "GNSS_TIME",
                                        "NETWORK_TIME",
                                        "NTP_TIME",
                                        "FS_TIME",
                                        "NO_TIME",
                                        "EXT_TIME",
                                        };

int LOG_UNIXTIME(const int32_t ln){
    int ret = 0;
    int32_t ct;
    uint64_t unix_time_ms;
    ret = date_time_now(&unix_time_ms);
    if(ret < 0){return ret;}
    ct = (uint32_t) (unix_time_ms/1000);

    printk("%d: UNIX TIME: %d s\n", ln, ct);


    return 0;
}

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

const char* const get_time_source_str(const uint8_t index)
{
    if (index < 6)  return time_source_str[index];
    else            return NULL;
}

