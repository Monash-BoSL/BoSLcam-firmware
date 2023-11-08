
#include <zephyr.h>
#include <device.h>
#include <sys/util.h>
#include <sys/printk.h>
#include <inttypes.h>
#include <logging/log.h>

// #include <drivers/spi.h>

#include <nrfx.h>

// #include <hal/nrf_timer.h>
// #include <hal/nrf_dppi.h>
// #include <hal/nrf_gpiote.h>
// #include <hal/nrf_gpio.h>
#include <hal/nrf_spim.h>

#include "sram.h"
#include "common.h"


LOG_MODULE_REGISTER(sram);

extern const struct device * spi_sram;


uint8_t txbf[8];
uint8_t rxbf[8];
void sram_test(){
	int ret;
	
    NRF_SPIM0->ENABLE = 1;

	nrf_spim_enable(NRF_SPIM0);

	nrf_spim_pins_set(NRF_SPIM0,
	        22,//sck
        	24,//mosi
        	23 //miso
	);

	nrf_spim_frequency_set(NRF_SPIM0, NRF_SPIM_FREQ_1M);



	nrf_spim_tx_buffer_set(NRF_SPIM0, txbf, sizeof(txbf));
	nrf_spim_rx_buffer_set(NRF_SPIM0, rxbf, sizeof(rxbf));
	
	nrf_spim_configure(NRF_SPIM0, NRF_SPIM_MODE_0, NRF_SPIM_BIT_ORDER_MSB_FIRST);

}