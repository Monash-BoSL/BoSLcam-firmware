
#include <zephyr.h>
#include <device.h>
#include <sys/util.h>
#include <sys/printk.h>
#include <inttypes.h>
#include <logging/log.h>

#include <drivers/i2c.h>
#include <drivers/gpio.h>

#include <nrfx.h>

#include <hal/nrf_timer.h>
#include <hal/nrf_dppi.h>
#include <hal/nrf_gpiote.h>
#include <hal/nrf_gpio.h>

#include "ov7675.h"
#include "ov7675_regs.h"
#include "common.h"


LOG_MODULE_REGISTER(ov7675);

extern const struct device * gpio;
extern const struct device * i2c_sccb;

void wr_reg(uint8_t reg,uint8_t dat){
	i2c_reg_write_byte(i2c_sccb, OV7670_I2C_ADDRESS, reg, dat);
}

uint8_t rd_reg(uint8_t reg){
	uint8_t val;
	i2c_reg_read_byte(i2c_sccb, OV7670_I2C_ADDRESS, reg, &val);
	return val;
}
static void wr_sensor_regs8_8(const struct regval_list reglist[]){
	const struct regval_list *next = reglist;
	for(;;){
		uint8_t reg_addr = next->reg_num;
		uint8_t reg_val = next->value;
		if((reg_addr==255)&&(reg_val==255)){break;}
		wr_reg(reg_addr, reg_val);
		next++;
	}
}
void set_color_space(enum COLORSPACE color){
	switch(color){
		case YUV422:
			wr_sensor_regs8_8(yuv422_ov7670);
		break;
		case RGB565:
			wr_sensor_regs8_8(rgb565_ov7670);
			{uint8_t temp=rd_reg(0x11);
			k_msleep(1);
			wr_reg(0x11,temp);}//according to the Linux kernel driver rgb565 PCLK needs rewriting
		break;
		case BAYER_RGB:
			wr_sensor_regs8_8(bayerRGB_ov7670);
		break;
	}
}
void set_res(enum RESOLUTION res){
	switch(res){
		case VGA:
			wr_reg(REG_COM3,0);	// REG_COM3
			wr_sensor_regs8_8(vga_ov7670);
		break;
		case QVGA:
			wr_reg(REG_COM3,4);	// REG_COM3 enable scaling
			wr_sensor_regs8_8(qvga_ov7670);
		break;
		case QQVGA:
			wr_reg(REG_COM3,4);	// REG_COM3 enable scaling
			wr_sensor_regs8_8(qqvga_ov7670);
		break;
	}
}

void ov7675_init(uint32_t auto_time){
	gpio = device_get_binding(DT_LABEL(DT_NODELABEL(gpio0)));
	LOG_INF("bind %s\n", gpio->name);
	i2c_sccb = device_get_binding(DT_LABEL(DT_NODELABEL(i2c2)));
	LOG_INF("bind %s\n", i2c_sccb->name);
	
	//setup gpio for all pins
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
	nrf_gpiote_task_configure(	NRF_GPIOTE,
								GPIOTE_CLK_TSK,
								SCCB_XCLK,
								NRF_GPIOTE_POLARITY_TOGGLE,
								NRF_GPIOTE_INITIAL_VALUE_HIGH);
    nrf_gpiote_task_enable(NRF_GPIOTE,GPIOTE_CLK_TSK);
	
	nrf_timer_task_trigger(NRF_TIMER0,NRF_TIMER_TASK_START);

    /* Enable DPPI Channel */
    nrf_dppi_channels_enable(NRF_DPPIC, 0x01 << SCCB_CLK_DPPI_CH);

	
	wr_reg(0x11,0);//set clock divider to 1, no need to slow it down!
	
	
	wr_reg(0x12, 0x80);//Reset the camera.
	k_msleep(100);
	wr_sensor_regs8_8(ov7675_qvga_regs);
	wr_reg(REG_COM10,32);//PCLK does not toggle on HBLANK.
	
	if(auto_time){
		k_msleep(auto_time);//delay for autoexposure awb, etc ...
	}
}

void ov7675_capture(uint8_t* buffer){
	uint16_t wg = IMAGE_WIDTH;//line width in pixels
	uint16_t hg = IMAGE_HEIGHT;//number of lines per frame
	uint16_t lg2;
	uint32_t p = 0;

	LOG_INF("ready\n");
	//Wait for vsync 
	while(!nrf_gpio_pin_read(SCCB_VS));//wait for high
	while(nrf_gpio_pin_read(SCCB_VS));//wait for low
	
	while(hg--){//get line
		lg2=wg;
		// printk("%u\n", hg);
		while(lg2--){//get pixel
			//low byte
			while(NRF_P0->IN & (0x1 << SCCB_PCLK));//wait for high on PCLK
			buffer[p+1] = (uint8_t) NRF_P0->IN;//read in D0 - D8
			while(!(NRF_P0->IN & (0x1 << SCCB_PCLK)));//wait for low on PCLK
			//high byte
			while(NRF_P0->IN & (0x1 << SCCB_PCLK));//wait for high on PCLK
			buffer[p] = (uint8_t) NRF_P0->IN;//read in D0 - D8
			while(!(NRF_P0->IN & (0x1 << SCCB_PCLK)));//wait for low on PCLK
			
			p += 2;
		}
		while((nrf_gpio_pin_read(SCCB_HREF)));//SYNC line on HREF
	}

	// //due to hardware error we need to swap the last 2 bits of buffer
	// for(uint32_t p = 0; p < IMAGE_SIZE_BYTES; p++){
		// uint8_t x = buffer[p];
		
		// buffer[p] = (x & ~(0x3)) | ((x >> 0x1)&0x1) | ((x << 1)&0x2);
	// }
}
