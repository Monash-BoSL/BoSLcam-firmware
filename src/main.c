/*
 * Copyright (c) 2016 Open-RnD Sp. z o.o.
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <nrfx.h>
#include <hal/nrf_timer.h>
#include <hal/nrf_dppi.h>
#include <hal/nrf_gpiote.h>
#include <hal/nrf_gpio.h>
#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <sys/util.h>
#include <sys/printk.h>
#include <inttypes.h>

#include <drivers/i2c.h>

// #include <i2c.h>

#define SLEEP_TIME_MS	1


#define CAMADDR_WR  0x42
#define CAMADDR_RD  0x43

#define SCCB_VS		11
#define SCCB_HREF	12
#define SCCB_PCLK	13
#define SCCB_XCLK	14
#define SCCB_PEN	15
#define SCCB_PDN	16


/*
 * Get button configuration from the devicetree sw0 alias. This is mandatory.
 */
#define SW0_NODE	DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios,
							      {0});
static struct gpio_callback button_cb_data;

/*
 * The led0 devicetree alias is optional. If present, we'll use it
 * to turn on the LED whenever the button is pressed.
 */
static struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios,
						     {0});

static const struct device * gpio; 

static const struct device *i2c_sccb;


void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
}

void main(void)
{
	int ret;
	
	gpio = device_get_binding(DT_LABEL(DT_NODELABEL(gpio0)));
	printk("bind %s\n", gpio->name);
	
	gpio_pin_configure(gpio, SCCB_PEN, GPIO_OUTPUT);
	gpio_pin_configure(gpio, SCCB_PDN, GPIO_OUTPUT);
	gpio_pin_set_raw(gpio, SCCB_PEN, 1);
	gpio_pin_set_raw(gpio, SCCB_PDN, 0);
		
		
	NRF_P0->PIN_CNF[18] = 0b00000000000000000000000000000001; 
	// nrf_gpio_cfg_input(21,NRF_GPIO_PIN_PULLUP);

	nrf_timer_frequency_set(NRF_TIMER0, NRF_TIMER_FREQ_125kHz);
	nrf_timer_mode_set(NRF_TIMER0, NRF_TIMER_MODE_TIMER);
	nrf_timer_cc_set(NRF_TIMER0,NRF_TIMER_CC_CHANNEL0, 0x10);
	nrf_timer_bit_width_set(NRF_TIMER0, NRF_TIMER_BIT_WIDTH_8);
	nrf_timer_shorts_set(NRF_TIMER0, 0b00000000000000000000000000000001);//reset timer on match with compare 0
	nrf_timer_publish_set(NRF_TIMER0, NRF_TIMER_EVENT_COMPARE0,0);
	// // nrf_gpiote_subscribe_set(NRF_GPIOTE, NRF_GPIOTE_TASK_OUT_0 ,0);
	// // nrf_gpiote_task_configure(NRF_GPIOTE,
                                                 // // 0,
                                                 // // 18,
                                                 // // NRF_GPIOTE_POLARITY_TOGGLE,
                                                 // // NRF_GPIOTE_INITIAL_VALUE_HIGH);

	// // nrf_dppi_task_trigger(NRF_DPPIC, NRF_DPPI_TASK_CHG0_EN);
	nrf_timer_task_trigger(NRF_TIMER0,NRF_TIMER_TASK_START);
	
	// uint8_t chen = nrf_dppi_channel_check(NRF_DPPIC, 0);
	// printk("print %i", chen);
	
	
	/* Configure GPIOTE Index 0 to be an Event */
    // nrf_gpiote_event_configure(NRF_GPIOTE,0,21,NRF_GPIOTE_POLARITY_HITOLO);

    /* Configure GPIOTE Index 1 to be a Task*/
    nrf_gpiote_task_configure(NRF_GPIOTE,1,18,NRF_GPIOTE_POLARITY_TOGGLE,NRF_GPIOTE_INITIAL_VALUE_LOW);

    /* Index 0 will Publish on DPPI Channel 0 */
    // nrf_gpiote_publish_set(NRF_GPIOTE,NRF_GPIOTE_EVENT_IN_0,0);

    /* Index 1 will Subscribe on DPPI Channel 0 */
    nrf_gpiote_subscribe_set(NRF_GPIOTE,NRF_GPIOTE_TASK_OUT_1,0);

    /* Enable Publish and Subscribe */
    nrf_gpiote_event_enable(NRF_GPIOTE,0);
    nrf_gpiote_task_enable(NRF_GPIOTE,1);

    /* Enable DPPI Channel */
    nrf_dppi_channels_enable(NRF_DPPIC, 1);

    /* Validate things did get enabled */
    printk("[0] is %d\n",nrf_gpiote_te_is_enabled(NRF_GPIOTE,0));
    // printk("[1] is %d\n",nrf_gpiote_te_is_enabled(NRF_GPIOTE,1));
    printk("[2] is %d\n",nrf_dppi_channel_check(NRF_DPPIC,0));
	
	while(1){
		
	}
	
	
	
	// while(1){
		// // NRF_GPIO->OUT |= (0x01 << 18);
		// // NRF_GPIO->OUT &= ~(0x01 << 18);
		// NRF_P0->OUTSET = (0x01 << 18);
		// NRF_P0->OUTCLR = (0x01 << 18);
		// NRF_P0->OUTSET = (0x01 << 18);
		// NRF_P0->OUTCLR = (0x01 << 18);
		// NRF_P0->OUTSET = (0x01 << 18);
		// NRF_P0->OUTCLR = (0x01 << 18);
		// NRF_P0->OUTSET = (0x01 << 18);
		// NRF_P0->OUTCLR = (0x01 << 18);
		// NRF_P0->OUTSET = (0x01 << 18);
		// NRF_P0->OUTCLR = (0x01 << 18);
		// NRF_P0->OUTSET = (0x01 << 18);
		// NRF_P0->OUTCLR = (0x01 << 18);
		// NRF_P0->OUTSET = (0x01 << 18);
		// NRF_P0->OUTCLR = (0x01 << 18);
		// NRF_P0->OUTSET = (0x01 << 18);
		// NRF_P0->OUTCLR = (0x01 << 18);
		// NRF_P0->OUTSET = (0x01 << 18);
		// NRF_P0->OUTCLR = (0x01 << 18);
		// NRF_P0->OUTSET = (0x01 << 18);
		// NRF_P0->OUTCLR = (0x01 << 18);
	// }
	
	i2c_sccb = device_get_binding(DT_LABEL(DT_NODELABEL(i2c2)));
	printk("bind %s\n", i2c_sccb->name);
	
	while(1){
		uint8_t buffer[40];
		// ret = i2c_write(i2c_sccb, buffer, 40, 0x03);
		ret = i2c_read(i2c_sccb, buffer, 1, CAMADDR_RD);
		printk("buf: ");
		for(int i = 0; i < 40; i++){
			printk("%02X", buffer[i]);
			buffer[i] = 0x00;
		}
		printk("\n");
		
		k_msleep(500);
	}
	

	if (!device_is_ready(button.port)) {
		printk("Error: button device %s is not ready\n",
		       button.port->name);
		return;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n",
		       ret, button.port->name, button.pin);
		return;
	}

	ret = gpio_pin_interrupt_configure_dt(&button,
					      GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, button.port->name, button.pin);
		return;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
	printk("Set up button at %s pin %d\n", button.port->name, button.pin);

	if (led0.port && !device_is_ready(led0.port)) {
		printk("Error %d: led0 device %s is not ready; ignoring it\n",
		       ret, led0.port->name);
		led0.port = NULL;
	}
	if (led0.port) {
		ret = gpio_pin_configure_dt(&led0, GPIO_OUTPUT);
		if (ret != 0) {
			printk("Error %d: failed to configure LED device %s pin %d\n",
			       ret, led0.port->name, led0.pin);
			led0.port = NULL;
		} else {
			printk("Set up LED at %s pin %d\n", led0.port->name, led0.pin);
		}
	}
	

	printk("Press the button\n");
	if (led0.port) {
		while (1) {
			/* If we have an LED, match its state to the button's. */
			int val = gpio_pin_get_dt(&button);
			if (val >= 0) {
				gpio_pin_set_dt(&led0, val);
			}
			k_msleep(SLEEP_TIME_MS);
		}
	}
}
