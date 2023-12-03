
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

#define SRAM_SIZE  		(0x20000)
#define SRAM_PAGE_SIZE  (32)


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
    int ret;
    uint8_t txdat[2] = {SRAM_INSTR_RDMR};
    struct spi_buf tx_buf = {.buf = txdat, .len = sizeof(txdat)};
    struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1};

    uint8_t rxdat[2];
    struct spi_buf rx_buf = {.buf = rxdat, .len = sizeof(rxdat)};
    struct spi_buf_set rx_bufs = {.buffers = &rx_buf, .count = 1};

    ret = spi_transceive(spi_sram, &spi_cfg, &tx_bufs, &rx_bufs);
    if(ret){return ret;}

    *mode = rxdat[1];


    return ret;
}

int sram_wrmr(uint8_t mode){
    int ret;

    uint8_t txdat[2] = {SRAM_INSTR_WRMR, mode};
    struct spi_buf tx_buf = {.buf = txdat, .len = sizeof(txdat)};
    struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1};

    ret = spi_transceive(spi_sram, &spi_cfg, &tx_bufs, NULL);
    if(ret){return ret;}

    return ret;
}

//data must of size at least SRAM_PAGE_SIZE
int sram_read_page(uint8_t* addr, uint8_t* data){
    int ret;
    ret = sram_wrmr(SRAM_MODE_PAGE);
    if(ret){return ret;}

    //if we have the buffers the same size then there is not a gap between reading and writing
    uint8_t txdat[4] = {SRAM_INSTR_READ,
                        (uint8_t)(0xFF & ((uintptr_t)addr >> (2*sizeof(uint8_t)*CHAR_BIT))),
                        (uint8_t)(0xFF & ((uintptr_t)addr >> (1*sizeof(uint8_t)*CHAR_BIT))),
                        (uint8_t)(0xFF & ((uintptr_t)addr >> (0*sizeof(uint8_t)*CHAR_BIT))),
                        };
    struct spi_buf tx_buf = {.buf = txdat, .len = sizeof(txdat)};
    struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1};

    uint8_t rxdat_dummy[sizeof(txdat)];
    struct spi_buf rx_cmd_buf = {.buf = rxdat_dummy, .len = sizeof(rxdat_dummy)};
    struct spi_buf rx_data_buf = {.buf = data, .len = SRAM_PAGE_SIZE};
    struct spi_buf rxdat_bufs[2] = {rx_cmd_buf,rx_data_buf};

    struct spi_buf_set rx_bufs = {.buffers = rxdat_bufs, .count = 2};

    ret = spi_transceive(spi_sram, &spi_cfg, &tx_bufs, &rx_bufs);
    if(ret){return ret;}

    return ret;
}

//data must of size at least 32
int sram_write_page(uint8_t* addr, uint8_t* data){
    int ret;
    ret = sram_wrmr(SRAM_MODE_PAGE);
    if(ret){return ret;}

    uint8_t txdat[4] = {
                        SRAM_INSTR_WRITE,
                        (uint8_t)(0xFF & ((uintptr_t)addr >> (2*sizeof(uint8_t)*CHAR_BIT))),
                        (uint8_t)(0xFF & ((uintptr_t)addr >> (1*sizeof(uint8_t)*CHAR_BIT))),
                        (uint8_t)(0xFF & ((uintptr_t)addr >> (0*sizeof(uint8_t)*CHAR_BIT)))
                        };
    struct spi_buf tx_cmd_buf = {.buf = txdat, .len = sizeof(txdat)};
    struct spi_buf tx_data_buf = {.buf = data, .len = SRAM_PAGE_SIZE};
    struct spi_buf txdat_bufs[2] = {tx_cmd_buf,tx_data_buf};
    struct spi_buf_set tx_bufs = {.buffers = txdat_bufs, .count = 2};

    ret = spi_transceive(spi_sram, &spi_cfg, &tx_bufs, NULL);
    return ret;
}

int sram_read_byte(uint8_t* addr, uint8_t* data){
    int ret;
    ret = sram_wrmr(SRAM_MODE_BYTE);
    if(ret){return ret;}

    //if we have the buffers the same size then there is not a gap between reading and writing
    uint8_t txdat[5] = {SRAM_INSTR_READ,
                        (uint8_t)(0xFF & ((uintptr_t)addr >> (2*sizeof(uint8_t)*CHAR_BIT))),
                        (uint8_t)(0xFF & ((uintptr_t)addr >> (1*sizeof(uint8_t)*CHAR_BIT))),
                        (uint8_t)(0xFF & ((uintptr_t)addr >> (0*sizeof(uint8_t)*CHAR_BIT))),
                        0x00
                        };
    struct spi_buf tx_buf = {.buf = txdat, .len = sizeof(txdat)};
    struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1};

    uint8_t rxdat[5];
    struct spi_buf rx_buf = {.buf = rxdat, .len = sizeof(rxdat)};
    struct spi_buf_set rx_bufs = {.buffers = &rx_buf, .count = 1};

    ret = spi_transceive(spi_sram, &spi_cfg, &tx_bufs, &rx_bufs);
    if(ret){return ret;}

    *data = rxdat[4];

    return ret;
}

int sram_write_byte(uint8_t* addr, uint8_t data){
    int ret;
    ret = sram_wrmr(SRAM_MODE_BYTE);
    if(ret){return ret;}

    uint8_t txdat[5] = {
                        SRAM_INSTR_WRITE,
                        (uint8_t)(0xFF & ((uintptr_t)addr >> (2*sizeof(uint8_t)*CHAR_BIT))),
                        (uint8_t)(0xFF & ((uintptr_t)addr >> (1*sizeof(uint8_t)*CHAR_BIT))),
                        (uint8_t)(0xFF & ((uintptr_t)addr >> (0*sizeof(uint8_t)*CHAR_BIT))),
                        data
                        };
    struct spi_buf tx_buf = {.buf = txdat, .len = sizeof(txdat)};
    struct spi_buf_set tx_bufs = {.buffers = &tx_buf, .count = 1};

    ret = spi_transceive(spi_sram, &spi_cfg, &tx_bufs, NULL);
    return ret;
}

void sram_test(){
    int ret;

    spi_sram = device_get_binding(DT_LABEL(DT_NODELABEL(spi1)));
    LOG_INF("bind %s\n", spi_sram->name);


    sram_wrmr(SRAM_MODE_BYTE);

    uint8_t txpage[SRAM_PAGE_SIZE];
    uint8_t rxpage[SRAM_PAGE_SIZE];

    for(size_t i = 0; i < SRAM_PAGE_SIZE; i++){
        txpage[i] = i;
    }

    sram_write_page((uint8_t*)0x000220, txpage);
    sram_write_byte((uint8_t*)0x000220, 0xDF);
    sram_write_byte((uint8_t*)0x000221, 0xA4);
    sram_write_byte((uint8_t*)0x000223, 0x9A);

    while (1) {
        led(1);
        LOG_INF("sending");

        sram_read_page((uint8_t*)0x0000220, rxpage);

        k_sleep(K_MSEC(300));
        led(0);

        k_sleep(K_MSEC(300));

    }

}