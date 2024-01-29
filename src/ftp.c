
#include <zephyr.h>
#include <device.h>


#include <sys/util.h>
#include <sys/printk.h>
#include <inttypes.h>
#include <logging/log.h>

#include <nrf_modem.h>
#include <nrf_modem_at.h>
#include <modem/nrf_modem_lib.h>
#include <modem/at_monitor.h>

#include <net/ftp_client.h>
#include <fs/fs.h>

#include <stdio.h>
#include <time.h>

#include "common.h"
#include "ftp.h"

LOG_MODULE_REGISTER(ftp);

void ftp_data_callback(const uint8_t *msg, uint16_t len)
{
    printk(msg);
}

void ftp_ctrl_callback(const uint8_t *msg, uint16_t len)
{
    printk(msg);
}

int ftp_mkdirs(const char* path) {
    int res;
    size_t pathlen = strlen(path)+1;
    if(pathlen > 256){return -ENAMETOOLONG;}//magic number of max path length

    char* current = k_malloc(pathlen);
    memset(current, '\0', pathlen);//null terminate

    char* pos = path;
    char* end = strchr(pos+1, '/');
    while(NULL != (end = strchr(pos+1, '/'))){
        strncpy(current+(pos-path), pos, end-pos);
        printk("mkdir %s\n", current);

        ftp_mkd(current);

        pos = end;
    }

    k_free(current);
    return res;//make sure that we return a nice error code here.
}

int modem_network_register(struct ftp_config_t* ftp_cfg_p){
    int ret;
    // char response[256];

    ret = nrf_modem_at_printf("AT");
    if(ret == 0){LOG_INF("AT initialised");}
    else if (ret < 0){LOG_ERR("AT initialisation error"); return ret;}

    ret = nrf_modem_at_printf("AT+CGDCONT=0,\"IP\",\"%s\"", ftp_cfg_p->apn);
    if(ret == 0){LOG_INF("CGDCONT ok");}
    else if (ret < 0){LOG_ERR("CGDCONT error"); return ret;}

    ret = nrf_modem_at_printf("AT+CFUN=1");
    if(ret == 0){LOG_INF("CFUN on ok");}
    else if (ret < 0){LOG_ERR("CFUN on error"); return ret;}

    //may get stuck here if there is no network...
    ret = nrf_modem_at_printf("AT+COPS=1,2,\"%s\"", ftp_cfg_p->network_operator);
    if(ret == 0){LOG_INF("COPS register ok");}
    else if (ret < 0){LOG_ERR("COPS register error"); return ret;}

    return 0;
}

int ftp_write_image(struct ftp_config_t* ftp_cfg_p, struct capture_t* capture){
    char path[MAX_PATH];
    int ret;

    LOG_INF("modem begin\n");

    if(strlen(ftp_cfg_p->image_path) > MAX_PATH + 12-1){//12 for unix time + extension
        LOG_ERR("file name too long");
        return -ENAMETOOLONG;
    }

    switch(capture->format){
        case BMP:
            sprintf(path, "%s%08X.bmp", ftp_cfg_p->image_path, capture->time);
        break;
        case JPG:
            sprintf(path, "%s%08X.jpg", ftp_cfg_p->image_path, capture->time);
        break;
    }

    // ret = nrf_modem_at_cmd(response, sizeof(response), "AT+CGMR");
    // printk(response);

    ret = modem_network_register(ftp_cfg_p);
    if (ret < 0){return ret;}

    // ret = nrf_modem_at_cmd(response, sizeof(response), "AT+CEREG?");
    // printk(response);

    // ret = nrf_modem_at_cmd(response, sizeof(response), "AT+CPIN?");
    // printk(response);

    ret = ftp_open(ftp_cfg_p->domain, 21, -1);
    if (ret < 0){return ret;}

    ret = ftp_login(ftp_cfg_p->username, ftp_cfg_p->password);
    if (ret < 0){return ret;}

    ret = ftp_mkdirs(path);

    ret = ftp_type(FTP_TYPE_BINARY);
    if (ret < 0){return ret;}

    struct fs_file_t imf;
    switch(capture->where){
        case SRAM:
            ret = ftp_put(path, image_resolutions[capture->resolution].bmp_header, BMPIMAGEOFFSET, FTP_PUT_NORMAL);
            if (ret < 0){return ret;}

            ret = ftp_put(path, capture->data, capture->size, FTP_PUT_APPEND);
            if (ret < 0){return ret;}

        break;
        case DISK:
            fs_file_t_init(&imf);
            ret = fs_open(&imf, capture->fp, FS_O_READ);
            // printf("%s\n",capture->fp);
            int read_bytes;
            int first = 1;
            while((read_bytes = fs_read(&imf,capture->data, capture->capacity)) > 0){
                enum ftp_put_type put_type = first ? FTP_PUT_NORMAL : FTP_PUT_APPEND;
                ret = ftp_put(path, capture->data, read_bytes, put_type);
                if (ret < 0) { 
                    fs_close(&imf);
                    return ret;
                }
                first = 0;
            }
            fs_close(&imf);
        break;
    }

    // ftp status
    ret = ftp_close();
    LOG_INF("UPLOAD SEQUENCE ENDED");
    return ret;
}

// int ftp_write_jpg(struct ftp_config_t* ftp_cfg_p, struct capture_t* capture){
//     char path[MAX_PATH];
//     int ret;

//     LOG_INF("modem begin\n");

//     if(strlen(ftp_cfg_p->image_path) > MAX_PATH + 12-1){//12 for unix time + extension
//         LOG_ERR("file name too long");
//         return -ENAMETOOLONG;
//     }

//     sprintf(path, "%s%08X.jpg", ftp_cfg_p->image_path, capture->time);

//     ret = modem_network_register(ftp_cfg_p);
//     if (ret < 0){return ret;}

//     ret = ftp_open(ftp_cfg_p->domain, 21, -1);
//     if (ret < 0){return ret;}

//     ret = ftp_login(ftp_cfg_p->username, ftp_cfg_p->password);
//     if (ret < 0){return ret;}

//     ret = ftp_mkdirs(path);

//     ret = ftp_type(FTP_TYPE_BINARY);
//     if (ret < 0){return ret;}

//     ret = ftp_put(path, capture->data, capture->size, FTP_PUT_NORMAL);
//     if (ret < 0){return ret;}

//     // ftp status
//     ret = ftp_close();
//     LOG_INF("UPLOAD SEQUENCE ENDED");
//     return ret;
// }

int ftp_write_status(struct ftp_config_t* ftp_cfg_p, struct status_t* status){
    char statstr[MAX_PATH];
    int ret;
    struct tm cal;

    LOG_INF("modem begin\n");

    unix_date(&cal, status->system_time);
    strftime(statstr, MAX_PATH, "%Y/%m/%d-%H:%M:%S UTC" , &cal);
    sprintf(statstr+strlen(statstr), ",%s,%d,%d\n",
                                time_source_str[status->time_src],
                                status->captures,
                                status->battery_voltage);

    ret = modem_network_register(ftp_cfg_p);
    if (ret < 0){return ret;}

    ret = ftp_open(ftp_cfg_p->domain, 21, -1);
    if (ret < 0){return ret;}

    ret = ftp_login(ftp_cfg_p->username, ftp_cfg_p->password);
    if (ret < 0){return ret;}

    ret = ftp_mkdirs(ftp_cfg_p->status_path);
    ret = ftp_type(FTP_TYPE_BINARY);
    if (ret < 0){return ret;}

    ret = ftp_put(ftp_cfg_p->status_path, statstr, strlen(statstr), FTP_PUT_APPEND);
    if (ret < 0){return ret;}

    ret = ftp_close();
    LOG_INF("UPLOAD SEQUENCE ENDED");
    return ret;
}

void ftp_setup(void){
    ftp_init(ftp_ctrl_callback, ftp_data_callback);
}
