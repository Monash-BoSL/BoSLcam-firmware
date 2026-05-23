
#include <inttypes.h>


#include <net/ftp_client.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "common.h"
#include "ftp.h"
#include "modem.h"
#include "util.h"

LOG_MODULE_REGISTER(ftp);

void ftp_data_callback(const uint8_t *msg, const uint16_t len) {
    printk("%s", msg);//this can be disabled once a wrong password returns a fail from ftp_login
}

void ftp_ctrl_callback(const uint8_t *msg, const uint16_t len) {
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

int ftp_write_image(const struct ftp_config_t* const ftp_cfg_p, const struct capture_t* const capture){
    int ret = 0;
    char path[MAX_PATH];

    LOG_INF("modem begin");

    if(strlen(ftp_cfg_p->image_path) + 12 + 1 > sizeof(path)){//12 for unix time + extension
        LOG_ERR("file name too long");
        return -ENAMETOOLONG;
    }

    switch(capture->format){
        case BMP:
            sprintf(path, "%s%08X.bmp", ftp_cfg_p->image_path, capture->time_wall);
        break;
        case JPG:
            sprintf(path, "%s%08X.jpg", ftp_cfg_p->image_path, capture->time_wall);
        break;
        default:
            LOG_ERR("bad capture format (%d)", (int)(capture->format));
            return -1;
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

int ftp_write_status(const struct ftp_config_t* const ftp_cfg_p, const struct status_t* const status, const struct capture_task_t* const capture_task){
    int ret = 0;
    char statstr[MAX_PATH];

    LOG_INF("modem begin");

    ret = strfstatus(statstr, sizeof(statstr), status, capture_task);
    if (ret < 0){
        LOG_ERR("status string format fail (%d)", ret);
        return ret;
    }

    LOG_INF("logging status to ftp: %s", log_strdup(statstr));

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
