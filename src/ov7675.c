
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

#include <storage/disk_access.h>
#include <fs/fs.h>
#include <ff.h>


#include "ov7675.h"
#include "ov7675_regs.h"
#include "common.h"


LOG_MODULE_REGISTER(ov7675);

extern const struct device * gpio;
extern const struct device * i2c_sccb;


void wr_reg(uint8_t reg,uint8_t dat){
    i2c_reg_write_byte(i2c_sccb, OV7675_I2C_ADDRESS, reg, dat);
}

uint8_t rd_reg(uint8_t reg){
    uint8_t val;
    i2c_reg_read_byte(i2c_sccb, OV7675_I2C_ADDRESS, reg, &val);
    return val;
}

void se_reg(uint8_t reg,uint8_t flags){
    uint8_t val;
    i2c_reg_read_byte(i2c_sccb, OV7675_I2C_ADDRESS, reg, &val);
    val |= flags;
    i2c_reg_write_byte(i2c_sccb, OV7675_I2C_ADDRESS, reg, val);
}

void cl_reg(uint8_t reg,uint8_t flags){
    uint8_t val;
    i2c_reg_read_byte(i2c_sccb, OV7675_I2C_ADDRESS, reg, &val);
    val &= ~flags;
    i2c_reg_write_byte(i2c_sccb, OV7675_I2C_ADDRESS, reg, val);
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

/*
 * Store a set of start/stop values into the camera.
 */
static void ov7670_set_hw(int hstart, int hstop, int vstart, int vstop)
{
    unsigned char v;
/*
 * Horizontal: 11 bits, top 8 live in hstart and hstop.  Bottom 3 of
 * hstart are in href[2:0], bottom 3 of hstop in href[5:3].  There is
 * a mystery "edge offset" value in the top two bits of href.
 */
    wr_reg(REG_HSTART, (hstart >> 3) & 0xff);
    wr_reg(REG_HSTOP, (hstop >> 3) & 0xff);
    v = rd_reg(REG_HREF);
    v = (v & 0xc0) | ((hstop & 0x7) << 3) | (hstart & 0x7);
    k_msleep(10);
    wr_reg(REG_HREF, v);
/*
 * Vertical: similar arrangement, but only 10 bits.
 */
    wr_reg(REG_VSTART, (vstart >> 2) & 0xff);
    wr_reg(REG_VSTOP, (vstop >> 2) & 0xff);
    v = rd_reg(REG_VREF);
    v = (v & 0xf0) | ((vstop & 0x3) << 2) | (vstart & 0x3);
    k_msleep(10);
    wr_reg(REG_VREF, v);

}

void _ov7675_write_flag(uint8_t reg, uint8_t flag, int on){
    if(on){	se_reg(reg, flag);}
    else{	cl_reg(reg, flag);}
}

void ov7675_aec(int on){
    _ov7675_write_flag(REG_COM8, COM8_AEC, on);
}


void ov7675_agc(int on){
    // _ov7675_write_flag(REG_COM13, COM13_AGC, on);
    _ov7675_write_flag(REG_COM8, COM8_AGC, on);
}

void ov7675_awb(int on){
    _ov7675_write_flag(REG_COM8, COM8_AWB, on);
}

void ov7675_init(uint32_t auto_time, enum image_resolution resolution, enum image_format format, struct capture_t* capture){
    capture->resolution = resolution;
    capture->format = format;

    gpio = device_get_binding(DT_LABEL(DT_NODELABEL(gpio0)));
    LOG_INF("bind %s\n", gpio->name);
    i2c_sccb = device_get_binding(DT_LABEL(DT_NODELABEL(i2c2)));
    LOG_INF("bind %s\n", i2c_sccb->name);

    //setup gpio for all pins
    gpio_pin_configure(gpio, SCCB_PEN, GPIO_OUTPUT);
    gpio_pin_configure(gpio, SCCB_PDN, GPIO_OUTPUT);
    gpio_pin_configure(gpio, DBGPIN, GPIO_OUTPUT);
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


    // wr_reg(0x11,0);//set clock divider to 1, no need to slow it down!
    k_msleep(100);
    ////////////////////////////////////////////////////////////////////////////////
    struct ov7675_image_size *wsize = &ov7675_resolutions[resolution];

    wr_reg(REG_COM7, COM7_RESET);//Reset the camera.
    k_msleep(100);
    wr_sensor_regs8_8(ov7670_default_regs);

    uint8_t com7 = ov7670_fmt_rgb565[0].value;
    com7 |= wsize->com7_bit;
    wr_reg(REG_COM7, com7);

    uint8_t com10 = 0;
    com10 |= COM10_PCLK_HB;

    wr_reg(REG_COM10, com10);
    wr_sensor_regs8_8(ov7670_fmt_rgb565+1);

    ov7670_set_hw(wsize->hstart, wsize->hstop, wsize->vstart,
                                                wsize->vstop);
    if(wsize->regs){
        wr_sensor_regs8_8(wsize->regs);
    }

    wr_reg(REG_DBLV, DBLV_BYPASS);//maybe?
    wr_reg(REG_CLKRC, CLK_SCALE & 0x02);//set clock divider to 2, needed for vga, qvga works fine with just 0x01
    ////////////////////////////////////////////////////////////////////////////////

    if(auto_time){
        k_msleep(auto_time);//delay for autoexposure awb, etc ...
    }
}

#define EBADIMAGESIZE 2
int ov7675_capture(struct capture_t* capture){
    uint16_t wg = QVGA_WIDTH;//line width in pixels
    uint16_t hg = VGA_HEIGHT;//number of lines per frame
    uint16_t lg2;
    uint32_t p = 0;

    if(capture->resolution != QVGA){
        LOG_ERR("SRAM capture only supports QVGA");
        return -EBADIMAGESIZE;
    }

    LOG_INF("ready\n");
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
            //low byte
            while(NRF_P0->IN & (0x1 << SCCB_PCLK));//wait for high on PCLK
            capture->data[p+1] = (uint8_t) NRF_P0->IN;//read in D0 - D8
            while(!(NRF_P0->IN & (0x1 << SCCB_PCLK)));//wait for low on PCLK
            //high byte
            while(NRF_P0->IN & (0x1 << SCCB_PCLK));//wait for high on PCLK
            capture->data[p] = (uint8_t) NRF_P0->IN;//read in D0 - D8
            while(!(NRF_P0->IN & (0x1 << SCCB_PCLK)));//wait for low on PCLK

            p += 2;
        }
        }
        while((NRF_P0->IN & (0x1 << SCCB_HREF)));//SYNC line on HREF

    }

    // //due to hardware error we need to swap the last 2 bits of buffer
    // for(uint32_t p = 0; p < IMAGE_SIZE_BYTES; p++){
        // uint8_t x = buffer[p];

        // buffer[p] = (x & ~(0x3)) | ((x >> 0x1)&0x1) | ((x << 1)&0x2);
    // }
    return 0;
}

#define EBUFFERTOOSMALL 1
int ov7675_capture_sdhc_buffered(struct capture_t* capture){
    int ret;
    const uint16_t line_width = ov7675_resolutions[capture->resolution].width;//line width in pixels

    const uint16_t line_skip = ov7675_resolutions[capture->resolution].line_skip;
    const uint16_t physical_lines = VGA_HEIGHT;//number of lines per frame which get sent over SCCB
    const uint16_t logical_lines = physical_lines/line_skip;//number of lines per frame which form the image
    const uint16_t buffer_size_lines = capture->capacity/(RBG565_PIXEL_SIZE_BYTES*line_width);//number of lines of the image which the buffer can fit

    if(buffer_size_lines < 1){
        LOG_ERR("image buffer too small (<1 line)");
        return -EBUFFERTOOSMALL;
        }

    strcpy(capture->fp, SDHC_PATH(SCRATCH_FILE));

    struct fs_file_t imf;
    fs_file_t_init(&imf);
    fs_unlink(capture->fp);
    fs_open(&imf, capture->fp, FS_O_WRITE | FS_O_CREATE);
    // fs_truncate(&imf, BMPIMAGEOFFSET+(RBG565_PIXEL_SIZE_BYTES*logical_lines*line_width));
    fs_write(&imf, image_resolutions[capture->resolution].bmp_header, BMPIMAGEOFFSET);

    ov7675_aec(0);
    ov7675_agc(0);
    ov7675_awb(0);
    
    //this is to make sure the auto exposure settings have stuck.
    while(!nrf_gpio_pin_read(SCCB_VS));//wait for high
    while(nrf_gpio_pin_read(SCCB_VS));//wait for low

    LOG_INF("ready\n");
    uint16_t current_line = physical_lines;
    for(int16_t lines_remaining = logical_lines; lines_remaining > 0; lines_remaining -= buffer_size_lines){
        uint32_t buffer_index = 0;
        //Wait for vsync
        while(!nrf_gpio_pin_read(SCCB_VS));//wait for high
        while(nrf_gpio_pin_read(SCCB_VS));//wait for low

        for(uint16_t line = physical_lines; line > 0; line--){//get line
            if((line % line_skip != 0) || current_line < line || buffer_index >= capture->capacity){
                while(!(NRF_P0->IN & (0x1 << SCCB_HREF)));//SYNC line on HREF
            }else{
                current_line = line;
                for(uint16_t pixel = line_width; pixel > 0; pixel--){//get pixel
                    //low byte
                    while(NRF_P0->IN & (0x1 << SCCB_PCLK));//wait for high on PCLK
                    capture->data[buffer_index+1] = (uint8_t) NRF_P0->IN;//read in D0 - D8
                    while(!(NRF_P0->IN & (0x1 << SCCB_PCLK)));//wait for low on PCLK
                    //high byte
                    while(NRF_P0->IN & (0x1 << SCCB_PCLK));//wait for high on PCLK
                    capture->data[buffer_index] = (uint8_t) NRF_P0->IN;//read in D0 - D8
                    while(!(NRF_P0->IN & (0x1 << SCCB_PCLK)));//wait for low on PCLK
                    buffer_index += 2;
                }
            }
            while((NRF_P0->IN & (0x1 << SCCB_HREF)));//SYNC line on HREF
        }
        ret = fs_write(&imf, capture->data, buffer_index);
        capture->size = buffer_index;
    }
    fs_close(&imf);
    capture->where = DISK;

    ov7675_aec(1);
    ov7675_agc(1);
    ov7675_awb(1);

    return 0;
}

void __ov7675_capture_stop_test(struct capture_t* capture){
    uint16_t wg = QVGA_WIDTH;//line width in pixels
    uint16_t hg = VGA_HEIGHT;//number of lines per frame
    uint16_t lg2;
    uint32_t p = 0;

    struct fs_file_t imf;
    fs_file_t_init(&imf);
    fs_open(&imf, SDHC_PATH(SCRATCH_FILE), FS_O_WRITE | FS_O_CREATE);
    fs_write(&imf, image_resolutions[QVGA].bmp_header, BMPIMAGEOFFSET);


    LOG_INF("ready\n");
    //Wait for vsync
    while(!nrf_gpio_pin_read(SCCB_VS));//wait for high
    while(nrf_gpio_pin_read(SCCB_VS));//wait for low

    while(hg--){//get line
        lg2=wg;
        if(hg % 2){
            while(!(NRF_P0->IN & (0x1 << SCCB_HREF)));//SYNC line on HREF

            while((NRF_P0->IN & (0x1 << SCCB_HREF)));//SYNC line on HREF

        }else{
            while(lg2--){//get pixel
                //low byte
                while(NRF_P0->IN & (0x1 << SCCB_PCLK));//wait for high on PCLK
                capture->data[p+1] = (uint8_t) NRF_P0->IN;//read in D0 - D8
                while(!(NRF_P0->IN & (0x1 << SCCB_PCLK)));//wait for low on PCLK
                //high byte
                while(NRF_P0->IN & (0x1 << SCCB_PCLK));//wait for high on PCLK
                capture->data[p] = (uint8_t) NRF_P0->IN;//read in D0 - D8
                while(!(NRF_P0->IN & (0x1 << SCCB_PCLK)));//wait for low on PCLK

                p += 2;
            }

            gpio_pin_set_raw(gpio, DBGPIN, 1);

            while((NRF_P0->IN & (0x1 << SCCB_HREF)));//SYNC line on HREF
            // nrf_timer_task_trigger(NRF_TIMER0,NRF_TIMER_TASK_STOP);
            nrf_gpiote_task_disable(NRF_GPIOTE,GPIOTE_CLK_TSK);

            gpio_pin_set_raw(gpio, SCCB_XCLK, 0);//need to bring the clock low when stopped otherwise image corruption
            for(uint32_t i = 0; i < 0x4000; i++);
            for(uint32_t i = 0; i < 0x4000; i++);
            for(uint32_t i = 0; i < 0x4000; i++);
            // nrf_timer_task_trigger(NRF_TIMER0,NRF_TIMER_TASK_START);
            nrf_gpiote_task_enable(NRF_GPIOTE,GPIOTE_CLK_TSK);

            gpio_pin_set_raw(gpio, DBGPIN, 0);

        }

    }


    // 	// fs_write(&imf, buffer+ps, p-ps);



    fs_write(&imf, capture->data, QVGA_WIDTH*VGA_HEIGHT);

    fs_close(&imf);
}

