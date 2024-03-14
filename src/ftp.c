
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

extern struct status_t stats_global;

#define AT_CMD_BUFFER_SIZE (512)

#define MAX_OPERATORS (8)
struct operator_t {
    uint8_t status;
    char mccmnc[7];
    uint8_t netact;
    uint8_t rsrq;
    uint8_t rsrp;
};

uint8_t operators_len = 0;
struct operator_t operators[MAX_OPERATORS];





void ftp_data_callback(const uint8_t *msg, uint16_t len)
{
    // printk(msg);
}

void ftp_ctrl_callback(const uint8_t *msg, uint16_t len)
{
    // printk(msg);
}

int ftp_mkdirs(const char* path) {
    int res = 0;
    size_t pathlen = strlen(path)+1;
    if(pathlen > 256){return -ENAMETOOLONG;}//magic number of max path length

    char* current = k_malloc(pathlen);
    memset(current, '\0', pathlen);//null terminate

    char* pos = path;
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


int print_operators(void) {
    LOG_INF("operators length: %d", operators_len);

    for (int i = 0; i < operators_len; ++i) {
        LOG_INF("operator: %d, status: %d, MCCMNC: %s, netact: %d, rsrq: %d, rsrp: %d", 
                i, 
                operators[i].status,
                log_strdup(operators[i].mccmnc), 
                operators[i].netact,
                operators[i].rsrq,
                operators[i].rsrp
                );
    }
    return 0;
}

int store_operators(const char* response, const size_t response_size){
    char* start = NULL;
    char* end = NULL;
    size_t len = 0;

    operators_len = 0;

    start = strstr(response, "%COPS: ");//make sure we are reading the +COPS line
    if(start == NULL){return -1;}

    for(uint8_t i = 0; i < MAX_OPERATORS; i++){
        start = strstr(start, "(");//start of a operators listing
        if(start == NULL){return 0;}
        end   = strstr(start, ")");//end of an operator listing
        if(end == NULL){return -2;}

        len = end - start;//check that the matches are of reasonable length
        if(len > response_size){return -3;} 

        struct operator_t* o = &operators[operators_len];
        uint8_t matches = sscanf(start, "(%d,%*[^,],%*[^,],\"%6[^\"]\",%d)", &o->status, &o->mccmnc, &o->netact);
        if(matches == 3){
            o->rsrq = 0xFF;//not known or not detectable
            o->rsrp = 0xFF;//not known or not detectable
            operators_len++;
        } 
        else {return -4;}

        start = end;
    }
    return -5;
}

int cmp_operator_rsrq(const void *a, const void *b){
    int16_t rsrq_a = ((struct operator_t*)a)->rsrq;
    int16_t rsrq_b = ((struct operator_t*)b)->rsrq;

    if(rsrq_a == 99 && rsrq_b == 99){return 0;}
    if(rsrq_a == 99){return 1;}
    if(rsrq_b == 99){return -1;}

    return  rsrq_a > rsrq_b ? -1 : 
           (rsrq_b > rsrq_a ?  1 : 0);

}


int modem_network_search(void){
    int ret = false;
    char response[AT_CMD_BUFFER_SIZE];


    ret = nrf_modem_at_cmd(response, sizeof(response), "AT%%COPS=?");
    if(ret){return ret;}

    store_operators(response, sizeof(response));
    print_operators();

    for(uint8_t i = 0; i < operators_len; i++){
        struct operator_t* o = &operators[i];
        ret = modem_network_select(o->mccmnc);
        if(ret){continue;}
        
        char mccmnc_current[7] = "\0\0\0\0\0\0\0";
        modem_current_mccmnc(mccmnc_current, sizeof(mccmnc_current));
        if(strcmp(o->mccmnc,mccmnc_current)){
          LOG_ERR("operators do not match");
          continue;
        }

        modem_signal_strength(&o->rsrq, &o->rsrp);

    }

    qsort(operators, operators_len, sizeof(struct operator_t), cmp_operator_rsrq);  

    return 0;
}


int modem_wait_registration(const uint32_t timeout_ms){
    int ret = 0;
    const uint32_t retry_delay_ms = 250;//empirically measured
    const uint32_t attempts = timeout_ms/retry_delay_ms;

    for(int i = 0; i < attempts; i++){
        int stat = 0;
        ret = nrf_modem_at_scanf("AT+CEREG?", "+CEREG: %*d,%d", &stat);
        if(ret == 1){
            switch (stat){
                case 1:
                case 5:
                    LOG_INF("CREG: registered, %d", stat);
                    return 0;
                break;
                default:
                    k_msleep(retry_delay_ms);
                break;
            }
        }else{LOG_ERR("CEREG error"); return ret;}
    }
    LOG_INF("CREG: not registed");
    return -1; 
}

int modem_current_mccmnc(char* mccmnc){
    int ret = 0;
    ret = nrf_modem_at_scanf("AT%XMONITOR", "%%XMONITOR: %*[^,],%*[^,],%*[^,],\"%6[^\"]\",", mccmnc);
    if(ret == 1){
        return 0;
    }
    return -1;
}

int modem_signal_strength(uint8_t* rsrq_p, uint8_t* rsrp_p){
    int ret = 0;
    ret = nrf_modem_at_scanf("AT+CESQ", "+CESQ: %*d,%*d,%*d,%*d,%d,%d", rsrq_p, rsrp_p);
    if(ret != 2){
        *rsrq_p = 0xFF;
        *rsrp_p = 0xFF;
        return 0;
    }
    return -1;
}

int modem_network_select(const char* mccmnc){
    int ret = 0;
    int timeout_ms     = 150000;
    int retry_delay_ms = 250;//
    int attempts = (mccmnc == NULL) ? timeout_ms/retry_delay_ms : 1;

    if(mccmnc == NULL){
        ret = nrf_modem_at_printf("AT+COPS=0");
    }else{
        ret = nrf_modem_at_printf("AT+COPS=1,2,\"%s\"", mccmnc);
    }

    if(ret == 0){
        LOG_INF("COPS ok");
        ret = modem_wait_registration(timeout_ms);
        return ret;
    }
    else if (ret < 0){LOG_ERR("COPS error"); return ret;}

    return ret;
}

int modem_network_register(struct ftp_config_t* ftp_cfg_p){
    int ret = 0;

    if(stats_global.mccmnc[0] == '\0'){//if uninitialised
        strncpy(stats_global.mccmnc, ftp_cfg_p->mccmnc, sizeof(stats_global.mccmnc) -1 ); 
        stats_global.mccmnc[sizeof(stats_global.mccmnc) - 1] = '\0';
    }

    ret = nrf_modem_at_printf("AT");
    if(ret == 0){LOG_INF("AT initialised");}
    else if (ret < 0){LOG_ERR("AT initialisation error"); return ret;}

    ret = nrf_modem_at_printf("AT+CGDCONT=0,\"IP\",\"%s\"", ftp_cfg_p->apn);
    if(ret == 0){LOG_INF("CGDCONT ok");}
    else if (ret < 0){LOG_ERR("CGDCONT error"); return ret;}

    ret = nrf_modem_at_printf("AT+CFUN=1");
    if(ret == 0){LOG_INF("CFUN on ok");}
    else if (ret < 0){LOG_ERR("CFUN on error"); return ret;}

    //first connect to last used network
    LOG_INF("Registration attempt to: %s", log_strdup(stats_global.mccmnc));
    ret = modem_network_select(stats_global.mccmnc);
    if(ret == 0){goto cleanup;}

    //then try connect to the config network
    ret = modem_network_select(ftp_cfg_p->mccmnc);
    if(ret == 0){
        ret = nrf_modem_at_scanf("AT%XMONITOR", "%%XMONITOR: %*[^,],%*[^,],%*[^,],\"%6[^\"]\",", stats_global.mccmnc);
        if(ret != 1){//For some reason we didn't match and so for additional safety we will revert to config
            strncpy(stats_global.mccmnc, ftp_cfg_p->mccmnc, sizeof(stats_global.mccmnc) -1 ); 
            stats_global.mccmnc[sizeof(stats_global.mccmnc) - 1] = '\0';
        }
        goto cleanup;
    }

    //then attempt automatic connection
    ret = modem_network_select(NULL);
    if(ret == 0){
        ret = nrf_modem_at_scanf("AT%XMONITOR", "%%XMONITOR: %*[^,],%*[^,],%*[^,],\"%6[^\"]\",", stats_global.mccmnc);
        if(ret != 1){//For some reason we didn't match and so for additional safety we will revert to config
            strncpy(stats_global.mccmnc, ftp_cfg_p->mccmnc, sizeof(stats_global.mccmnc) -1 ); 
            stats_global.mccmnc[sizeof(stats_global.mccmnc) - 1] = '\0';
        }
        goto cleanup;
    }

    if(!stats_global.network_searched){
        LOG_INF("Performing forced network search");
        modem_network_search();
        stats_global.network_searched = 1;//not we never do this again regardless of if the search was successfull
    }


    //attempt to connect to networks in order of signal strength
    for(uint8_t i = 0; i < operators_len; i++){
        struct operator_t* o = &operators[i];
        if(o->rsrq == 0xFF || o->rsrp == 0xFF){continue;}
        ret = modem_network_select(o->mccmnc);
        if(ret){continue;}

        ret = modem_current_mccmnc(stats_global.mccmnc);
        if(ret == 0){
            LOG_INF("Updating network preference to: %s", log_strdup(stats_global.mccmnc));
        }

        return 0; 
    }
 
    LOG_ERR("Unable to register to network");
    return -1;
cleanup:
    modem_signal_strength(&stats_global.rsrq, &stats_global.rsrp);//we ignore the error here as its not too important if the signal strength is bad; 
    return ret;
}

int modem_network_deregister(void){
    int ret = 0;

    // ret = nrf_modem_at_printf("AT");
    // if(ret == 0){LOG_INF("AT initialised");}
    // else if (ret < 0){LOG_ERR("AT initialisation error"); return ret;}

    ret = nrf_modem_at_printf("AT+CFUN=0");
    if(ret == 0){LOG_INF("CFUN off ok");}
    else if (ret < 0){LOG_ERR("CFUN off error"); return ret;}

    return 0;
}

int ftp_write_image(struct ftp_config_t* ftp_cfg_p, struct capture_t* capture){
    int ret = 0;
    char path[MAX_PATH];

    LOG_INF("modem begin");

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
    if (ret < 0){LOG_ERR("register err %d", ret); return ret;}
    else {LOG_INF("register ok");};

    // ret = nrf_modem_at_cmd(response, sizeof(response), "AT+CEREG?");
    // printk(response);

    // ret = nrf_modem_at_cmd(response, sizeof(response), "AT+CPIN?");
    // printk(response);

    ret = ftp_open(ftp_cfg_p->domain, 21, -1);
    if (ret < 0){LOG_ERR("open err %d", ret); goto cleanup;}
    else {LOG_INF("open ok");};

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
        case SRAM:
            ret = ftp_put(path, image_resolutions[capture->resolution].bmp_header, BMPIMAGEOFFSET, FTP_PUT_NORMAL);
            if (ret < 0){LOG_ERR("put err %d", ret); goto cleanup;}
            else {LOG_INF("put ok");};

            ret = ftp_put(path, capture->data, capture->size, FTP_PUT_APPEND);
            if (ret < 0){LOG_ERR("put err %d", ret); goto cleanup;}
            else {LOG_INF("put ok");};

        break;
        case DISK:
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
                                time_source_str[status->time_src],
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
