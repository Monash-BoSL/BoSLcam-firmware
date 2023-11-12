
#include <zephyr.h>
#include <device.h>
#include <sys/util.h>
#include <sys/printk.h>
#include <inttypes.h>
#include <logging/log.h>

#include <drivers/spi.h>
#include <drivers/uart.h>
#include <drivers/gpio.h>

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
extern const struct device * gpio;

const struct spi_config spi_cfg = {
    .frequency = 500000,
    .operation = SPI_OP_MODE_MASTER
				 | SPI_TRANSFER_MSB
				 | SPI_WORD_SET(8)
				 | SPI_MODE_CPOL
				 | SPI_MODE_CPHA,
    // .cs = SPI_CS_CONTROL_PTR_DT(DT_NODELABEL(spidev), 10),
    // .cs = SPI_CS_CONTROL_PTR_DT(DT_NODELABEL(spidev), 0),
    .cs = NULL,
};


void sram_test(){
	int ret;

	gpio = device_get_binding(DT_LABEL(DT_NODELABEL(gpio0)));
	gpio_pin_configure(gpio, 28, GPIO_OUTPUT);
	gpio_pin_set_raw(gpio, 28, 0);


	spi_sram = device_get_binding(DT_LABEL(DT_NODELABEL(spi1)));
	LOG_INF("bind %s\n", spi_sram->name);

	uint8_t txdatwr[2] = {0b00000001, 0b10000000};
    struct spi_buf tx_bufwr = {.buf = txdatwr, .len = sizeof(txdatwr)};
    struct spi_buf_set tx_bufswr = {.buffers = &tx_bufwr, .count = 1};


	uint8_t txdat[1] = {0b00000101};
    struct spi_buf tx_buf = {.buf = txdat, .len = sizeof(txdat)};
    struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1};

	uint8_t rxdat[4];
	struct spi_buf rx_buf = {.buf = rxdat, .len = sizeof(rxdat)};
    struct spi_buf_set rx_bufs = {.buffers = &rx_buf, .count = 1};



	while (1) {
		led(1);
		LOG_INF("sending");
        
		gpio_pin_set_raw(gpio, 28, 0);
        spi_write(spi_sram, &spi_cfg, &tx_bufswr);
		gpio_pin_set_raw(gpio, 28, 1);
		k_sleep(K_MSEC(20));
		gpio_pin_set_raw(gpio, 28, 0);
        spi_transceive(spi_sram, &spi_cfg, &tx_bufs, &rx_bufs);
		gpio_pin_set_raw(gpio, 28, 1);
		
		printk("recieved: ");
		for(size_t i = 0; i < rx_bufs.buffers[0].len; i++){
			printk("%02hhX", ((uint8_t*)rx_bufs.buffers[0].buf)[i]);
			((uint8_t*)rx_bufs.buffers[0].buf)[i] = 0x0A;
		}
		printk("\n");

		k_sleep(K_MSEC(300));
		led(0);

		k_sleep(K_MSEC(300));

    }

}