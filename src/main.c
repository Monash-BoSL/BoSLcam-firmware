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

#include "ov7675.h"

// #include <i2c.h>

#define SLEEP_TIME_MS	1


#define CAMADDR_WR  0x42
#define CAMADDR_RD  0x43
#define CAMADDR		0x21

#define SCCB_VS		11
#define SCCB_HREF	12
#define SCCB_PCLK	13
#define SCCB_XCLK	14
#define SCCB_PEN	15
#define SCCB_PDN	16

#define SCCB_CLK_DPPI_CH 0
#define GPIOTE_CLK_TSK 0


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

const struct device * i2c_sccb;

uint8_t imbuf[640*240];

void button_pressed(const struct device *dev, struct gpio_callback *cb,
		    uint32_t pins)
{
	printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
}

void sccb_setup(void){
	gpio = device_get_binding(DT_LABEL(DT_NODELABEL(gpio0)));
	printk("bind %s\n", gpio->name);
	
	gpio_pin_configure(gpio, SCCB_PEN, GPIO_OUTPUT);
	gpio_pin_configure(gpio, SCCB_PDN, GPIO_OUTPUT);
	for(int i = 0; i < 8; i++){
		gpio_pin_configure(gpio, i, GPIO_INPUT);
	}
	gpio_pin_configure(gpio, SCCB_VS, GPIO_INPUT);
	gpio_pin_configure(gpio, SCCB_HREF, GPIO_INPUT);
	gpio_pin_configure(gpio, SCCB_PCLK, GPIO_INPUT);
	gpio_pin_set_raw(gpio, SCCB_PEN, 1);
	gpio_pin_set_raw(gpio, SCCB_PDN, 0);
				
	NRF_P0->PIN_CNF[SCCB_XCLK] = 0b00000000000000000000000000000001; 


	//setup clock on XCLK pin at 8 MHz.
	nrf_timer_frequency_set(NRF_TIMER0, NRF_TIMER_FREQ_16MHz);
	nrf_timer_mode_set(NRF_TIMER0, NRF_TIMER_MODE_TIMER);
	nrf_timer_cc_set(NRF_TIMER0,NRF_TIMER_CC_CHANNEL0, 0x01);
	nrf_timer_bit_width_set(NRF_TIMER0, NRF_TIMER_BIT_WIDTH_8);
	nrf_timer_shorts_set(NRF_TIMER0, 0b00000000000000000000000000000001);//reset timer on match with compare 0
	nrf_timer_publish_set(NRF_TIMER0, NRF_TIMER_EVENT_COMPARE0,SCCB_CLK_DPPI_CH);
	nrf_gpiote_subscribe_set(NRF_GPIOTE, NRF_GPIOTE_TASK_OUT_0 ,SCCB_CLK_DPPI_CH);
	nrf_gpiote_task_configure(NRF_GPIOTE,
								 GPIOTE_CLK_TSK,
								 SCCB_XCLK,
								 NRF_GPIOTE_POLARITY_TOGGLE,
								 NRF_GPIOTE_INITIAL_VALUE_HIGH);
    nrf_gpiote_task_enable(NRF_GPIOTE,GPIOTE_CLK_TSK);


	nrf_timer_task_trigger(NRF_TIMER0,NRF_TIMER_TASK_START);

    /* Enable DPPI Channel */
    nrf_dppi_channels_enable(NRF_DPPIC, 0x01 << SCCB_CLK_DPPI_CH);


	
	i2c_sccb = device_get_binding(DT_LABEL(DT_NODELABEL(i2c2)));
	printk("bind %s\n", i2c_sccb->name);

	camInit();	
	wrReg(REG_CLKRC, CLK_SCALE & 0x00);//set clock divider to 1, no need to slow it down!
	

	k_msleep(1000);//delay for autoexposure awb, etc ...
}

void get_frame(void){
	uint16_t wg = 640;//640;//line width in bytes
	uint16_t hg = 480;//240;//number of lines per frame
	uint16_t lg2;
	uint32_t p = 0;

	printk("RDY\n");
	//Wait for vsync 
	while(!nrf_gpio_pin_read(SCCB_VS));//wait for high
	while(nrf_gpio_pin_read(SCCB_VS));//wait for low

	while(hg--){//get line
		lg2=wg;
		// printk("%u\n", hg);
		if(hg % 2){
			while(!(NRF_P0->IN & (0x1 << SCCB_HREF)));//SYNC line on HREF
		}else{
		while(lg2--){//get pixel
			while(NRF_P0->IN & (0x1 << SCCB_PCLK));//wait for high on PCLK
			imbuf[p] = (uint8_t) NRF_P0->IN;//read in D0 - D8
			while(!(NRF_P0->IN & (0x1 << SCCB_PCLK)));//wait for low on PCLK
			p++;
		}
		}
		while((NRF_P0->IN & (0x1 << SCCB_HREF)));//SYNC line on HREF
	}

	// //due to hardware error we need to swap the last 2 bits of imbuf
	// for(uint32_t p = 0; p < 640*240; p++){
		// uint8_t x = imbuf[p];
		
		// imbuf[p] = (x & ~(0x3)) | ((x >> 0x1)&0x1) | ((x << 1)&0x2);
	// }
}

void main(void)
{
	int ret;
	
	sccb_setup();

	get_frame();

	printk("+++image end");






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
