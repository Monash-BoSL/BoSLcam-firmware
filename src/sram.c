
#include <zephyr.h>
#include <device.h>
#include <sys/util.h>
#include <sys/printk.h>
#include <inttypes.h>
#include <logging/log.h>

#include <drivers/spi.h>

#include <nrfx.h>

// #include <hal/nrf_timer.h>
// #include <hal/nrf_dppi.h>
// #include <hal/nrf_gpiote.h>
// #include <hal/nrf_gpio.h>
#include <hal/nrf_spim.h>
#include <hal/nrf_uarte.h>

#include "sram.h"
#include "common.h"


LOG_MODULE_REGISTER(sram);

extern const struct device * spi_sram;


const struct spi_config spi_cfg = {
    .frequency = 500000,
    .operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8),
    .cs = SPI_CS_CONTROL_PTR_DT(DT_NODELABEL(spidev), 10),
};


void sram_test(){
	int ret;
	spi_sram = device_get_binding(DT_LABEL(DT_NODELABEL(spi0)));
	LOG_INF("bind %s\n", spi_sram->name);

    NRF_UARTE0->ENABLE = 0;
	nrf_uarte_disable(NRF_UARTE0);
    NRF_SPIM0->ENABLE = 1;
	nrf_spim_enable(NRF_SPIM0);
	nrf_spim_pins_set(NRF_SPIM0,
	        26,//sck
        	27,//mosi
        	29 //miso
	);
	// nrf_spim_frequency_set(NRF_SPIM0, NRF_SPIM_FREQ_1M);


	uint8_t cmd = 0XA5;
    struct spi_buf tx_buf = {.buf = &cmd, .len = 1};
    struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1};


	while (1) {
		led(1);
		LOG_INF("sending");
        spi_write(spi_sram, &spi_cfg, &tx_bufs);
        k_sleep(K_MSEC(300));
		led(0);
		k_sleep(K_MSEC(300));
    }

}