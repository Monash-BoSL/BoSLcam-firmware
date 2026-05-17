
#include <inttypes.h>


#include <net/ftp_client.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "common.h"
#include "ftp.h"
#include "util.h"

LOG_MODULE_REGISTER(ftp);

void ftp_data_callback(const uint8_t *msg, uint16_t len)
{
    printk("%s", msg);//this can be disabled once a wrong password returns a fail from ftp_login
}

void ftp_ctrl_callback(const uint8_t *msg, uint16_t len)
{
    printk("%s", msg);
}

int ftp_mkdirs(const char* path) {
    int res = 0;
    size_t pathlen = strlen(path)+1;
    if(pathlen > 256){return -ENAMETOOLONG;}//magic number of max path length

    char* current = k_malloc(pathlen);
    memset(current, '\0', pathlen);//null terminate

    const char* pos = path;
    char* end = strchr(pos+1, '/');
    while(NULL != (end = strchr(pos+1, '/'))){
        strncpy(current+(pos-path), pos, end-pos);
        // printk("mkdir %s\n", current);

        ftp_mkd(current);

        pos = end;
    }

    k_free(current);
    return res;//make sure that we return a nice error code here.
}

int ftp_write_image(struct ftp_config_t* ftp_cfg_p, struct capture_t* capture){
    int ret = 0;
    char path[MAX_PATH];

    LOG_INF("modem begin");

    if(strlen(ftp_cfg_p->image_path) + 12 + 1 > sizeof(path)){//12 for unix time + extension
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
    if (ret < 0){LOG_ERR("register err %d", ret); return ret;}
    else {LOG_INF("register ok");};

    // ret = nrf_modem_at_cmd(response, sizeof(response), "AT+CEREG?");
    // printk(response);

    // ret = nrf_modem_at_cmd(response, sizeof(response), "AT+CPIN?");
    // printk(response);

    ret = ftp_open(ftp_cfg_p->domain, 21, -1);
    if (ret < 0){LOG_ERR("open err %d", ret); goto cleanup;}
    else {LOG_INF("open ok");};

    //check if ret=530 (AUTHENTICATION ERROR)
    ret = ftp_login(ftp_cfg_p->username, ftp_cfg_p->password);
    if (ret < 0){LOG_ERR("login err %d", ret); goto cleanup;}
    else {LOG_INF("login ok");};

    ret = ftp_mkdirs(path);
    if (ret < 0){LOG_ERR("mkdirs err %d", ret); goto cleanup;}
    else {LOG_INF("mkdirs ok");};

    ret = ftp_type(FTP_TYPE_BINARY);
    if (ret < 0){LOG_ERR("type binary err %d", ret); goto cleanup;}
    else {LOG_INF("type binary ok");};

    struct fs_file_t imf;
    switch(capture->where){
        case DATA_LOCATION_SRAM:
            ret = ftp_put(path, image_resolutions[capture->resolution].bmp_header, BMPIMAGEOFFSET, FTP_PUT_NORMAL);
            if (ret < 0){LOG_ERR("put err %d", ret); goto cleanup;}
            else {LOG_INF("put ok");};

            ret = ftp_put(path, capture->data, capture->size, FTP_PUT_APPEND);
            if (ret < 0){LOG_ERR("put err %d", ret); goto cleanup;}
            else {LOG_INF("put ok");};

            break;
        case DATA_LOCATION_DISK:
            fs_file_t_init(&imf);
            ret = fs_open(&imf, capture->fp, FS_O_READ);
            int read_bytes;
            int first = 1;
            while((read_bytes = fs_read(&imf,capture->data, capture->capacity)) > 0){
                enum ftp_put_type put_type = first ? FTP_PUT_NORMAL : FTP_PUT_APPEND;
                ret = ftp_put(path, capture->data, read_bytes, put_type);
                if (ret < 0) { 
                    LOG_INF("put err %d", ret); 
                    fs_close(&imf);
                    goto cleanup;
                } else {
                    ret = 0;
                }
                first = 0;
            }
            LOG_INF("put ok");
            fs_close(&imf);
            break;
        case DATA_LOCATION_NONE:
            LOG_ERR("data location not specified");
            break;
        default:
            LOG_ERR("invalid data location (%d)", (int)capture->where);
            break;
    }

cleanup:
    LOG_INF("FTP STATUS: closing...");
    ftp_close();
    // LOG_INF("UPLOAD SEQUENCE ENDED. ret: %d", ret);
    return ret;
}

int ftp_write_status(struct ftp_config_t* ftp_cfg_p, struct status_t* status){
    int ret = 0;
    char statstr[MAX_PATH];
    struct tm cal;

    LOG_INF("modem begin");

    unix_date(&cal, status->system_time);
    strftime(statstr, MAX_PATH, "%Y/%m/%d-%H:%M:%S UTC" , &cal);
    sprintf(statstr+strlen(statstr), ",%s,%d,%d,%s,%d,%d\n",
                                get_time_source_str(status->time_src),
                                status->captures,
                                status->battery_voltage,
                                status->mccmnc,
                                status->rsrq,
                                status->rsrp
                                );

    ret = modem_network_register(ftp_cfg_p);
    if (ret < 0){LOG_ERR("register err %d", ret); return ret;}
    else {LOG_INF("register ok");};

    ret = ftp_open(ftp_cfg_p->domain, 21, -1);
    if (ret < 0){LOG_ERR("open err %d", ret); goto cleanup;}
    else {LOG_INF("open ok");};

    //check if ret=530 (AUTHENTICATION ERROR)
    ret = ftp_login(ftp_cfg_p->username, ftp_cfg_p->password);
    if (ret < 0){LOG_ERR("login err %d", ret); goto cleanup;}
    else {LOG_INF("login ok");};

    ret = ftp_mkdirs(ftp_cfg_p->status_path);
    if (ret < 0){LOG_ERR("mkdirs err: %d", ret); goto cleanup;}
    else {LOG_INF("mkdirs ok");};

    ret = ftp_type(FTP_TYPE_BINARY);
    if (ret < 0){LOG_ERR("type binary err %d", ret); goto cleanup;}
    else {LOG_INF("type binary ok");};

    ret = ftp_put(ftp_cfg_p->status_path, statstr, strlen(statstr), FTP_PUT_APPEND);
    if (ret < 0){LOG_ERR("put err %d", ret); goto cleanup;}
    else {LOG_INF("put ok"); ret = 0;};

cleanup:
    LOG_INF("closing...");
    ftp_close();
    // LOG_INF("UPLOAD SEQUENCE ENDED. ret: %d", ret);
    return ret;
}

void ftp_setup(void){
    ftp_init(ftp_ctrl_callback, ftp_data_callback);
}
