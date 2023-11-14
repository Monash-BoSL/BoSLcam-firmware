
#include <zephyr.h>
#include <device.h>
#include <sys/util.h>
#include <sys/printk.h>
#include <inttypes.h>
#include <limits.h>
#include <logging/log.h>

#include <drivers/spi.h>
#include <drivers/uart.h>

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
    .operation =   SPI_OP_MODE_MASTER
				 | SPI_TRANSFER_MSB
				 | SPI_WORD_SET(8),
				//  | SPI_MODE_CPOL
				//  | SPI_MODE_CPHA,
    .cs = SPI_CS_CONTROL_PTR_DT(DT_NODELABEL(spi_sram0), 0),
};

#define SRAM_INSTR_READ (0x03)
#define SRAM_INSTR_WRITE (0x02)
#define SRAM_INSTR_EDIO (0x3B)
#define SRAM_INSTR_RSTIO (0xFF)
#define SRAM_INSTR_RDMR (0x05)
#define SRAM_INSTR_WRMR (0x01)

#define SRAM_MODE_BYTE (0b00000000)
#define SRAM_MODE_PAGE (0b10000000)
#define SRAM_MODE_SEQN (0b01000000)
#define SRAM_MODE_RESV (0b11000000)

int sram_rdmr(uint8_t* mode){
	uint8_t txdat[1] = {SRAM_INSTR_RDMR};
    struct spi_buf tx_buf = {.buf = txdat, .len = sizeof(txdat)};
    struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1};

	uint8_t rxdat[4];
	struct spi_buf rx_buf = {.buf = rxdat, .len = sizeof(rxdat)};
    struct spi_buf_set rx_bufs = {.buffers = &rx_buf, .count = 1};

	spi_transceive(spi_sram, &spi_cfg, &tx_bufs, &rx_bufs);

	printk("recieved: ");
	for(size_t i = 0; i < rx_bufs.buffers[0].len; i++){
		printk("%02hhX ", ((uint8_t*)rx_bufs.buffers[0].buf)[i]);
		((uint8_t*)rx_bufs.buffers[0].buf)[i] = 0x0A;
	}
	printk("\n");

}

int sram_wrmr(uint8_t mode){
	int ret;

	uint8_t txdat[2] = {SRAM_INSTR_WRMR, mode};
    struct spi_buf tx_buf = {.buf = txdat, .len = sizeof(txdat)};
    struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1};

	ret = spi_write(spi_sram, &spi_cfg, &tx_bufs);
	return ret;
}

int sram_read_byte(uint8_t* addr, uint8_t* data){
	int ret;
	ret = sram_wrmr(SRAM_MODE_BYTE);
	if(ret){return ret;}

	uint8_t txdat[4] = {SRAM_INSTR_READ, 
						(uint8_t)(0xFF & ((uintptr_t)addr >> (2*sizeof(uint8_t)*CHAR_BIT))),
						(uint8_t)(0xFF & ((uintptr_t)addr >> (1*sizeof(uint8_t)*CHAR_BIT))),
						(uint8_t)(0xFF & ((uintptr_t)addr >> (0*sizeof(uint8_t)*CHAR_BIT)))
						};
    struct spi_buf tx_buf = {.buf = txdat, .len = sizeof(txdat)};
    struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1};

	uint8_t rxdat[8];
	struct spi_buf rx_buf = {.buf = rxdat, .len = sizeof(rxdat)};
    struct spi_buf_set rx_bufs = {.buffers = &rx_buf, .count = 1};

		spi_transceive(spi_sram, &spi_cfg, &tx_bufs, &rx_bufs);
		
	printk("recieved: ");
	for(size_t i = 0; i < rx_bufs.buffers[0].len; i++){
		printk("%02hhX ", ((uint8_t*)rx_bufs.buffers[0].buf)[i]);
		((uint8_t*)rx_bufs.buffers[0].buf)[i] = 0x0A;
	}
	printk("\n");

}

int sram_write_byte(uint8_t* addr, uint8_t data){
	int ret;
	// ret = sram_wrmr(SRAM_MODE_BYTE);
	// if(ret){return ret;}

	uint8_t txdat[5] = {
						SRAM_INSTR_WRITE, 
						(uint8_t)(0xFF & ((uintptr_t)addr >> (2*sizeof(uint8_t)*CHAR_BIT))),
						(uint8_t)(0xFF & ((uintptr_t)addr >> (1*sizeof(uint8_t)*CHAR_BIT))),
						(uint8_t)(0xFF & ((uintptr_t)addr >> (0*sizeof(uint8_t)*CHAR_BIT))),
						data
						};
    struct spi_buf tx_buf = {.buf = txdat, .len = sizeof(txdat)};
    struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1};
	
	ret = spi_write(spi_sram, &spi_cfg, &tx_bufs);
	return ret;
}

void sram_test(){
	int ret;

	spi_sram = device_get_binding(DT_LABEL(DT_NODELABEL(spi1)));
	LOG_INF("bind %s\n", spi_sram->name);


	sram_wrmr(SRAM_MODE_BYTE);


	sram_write_byte((uint8_t*)0x0002F3, 0xDF);
	while (1) {
		led(1);
		LOG_INF("sending");
        
		// sram_wrmr(SRAM_MODE_PAGE);
		// sram_rdmr(NULL);

		sram_read_byte((uint8_t*)0x00002F3, NULL);

		k_sleep(K_MSEC(300));
		led(0);

		k_sleep(K_MSEC(300));

    }

}