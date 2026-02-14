
#include <nrf_socket.h>
#include <nrf_modem.h>
#include <nrf_modem_at.h>
#include <modem/nrf_modem_lib.h>
#include <modem/at_monitor.h>
// #include <modem/lte_lc.h>

#include <stdlib.h>

#include "common.h"
#include "modem.h"

LOG_MODULE_REGISTER(modem);

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

static int modem_signal_strength(uint8_t* rsrq_p, uint8_t* rsrp_p);
static int modem_current_mccmnc(char* mccmnc);
static int modem_network_select(const char* mccmnc);

/* 
    modem reset behaviour 
    should be used in conjunction with
    CONFIG_NRF_MODEM_LIB_ON_FAULT_RESET_MODEM=y
    Kconfig option
*/
#define MODEM_MAX_DAILY_SHUTDOWNS (3) // maximum times the modem may be shutdown/reset before crashing program
static K_SEM_DEFINE(modem_recent_shutdown_sem, 0, MODEM_MAX_DAILY_SHUTDOWNS); // semaphore to store the number of daily shutdowns

static void shutdown_decrementer_worker(void *p1, void *p2, void *p3)
{
    LOG_INF("shutdown decrementer worker active");
    while (true) {
        LOG_INF("semaphore eater sleeping for: %d s", (86400/MODEM_MAX_DAILY_SHUTDOWNS));
        k_sleep(K_SECONDS(86400/MODEM_MAX_DAILY_SHUTDOWNS));
        LOG_INF("taking semaphore. value: %d", k_sem_count_get(&modem_recent_shutdown_sem));
        k_sem_take(&modem_recent_shutdown_sem, K_NO_WAIT);
        LOG_INF("taking semaphore taken. new value: %d", k_sem_count_get(&modem_recent_shutdown_sem));
    }
}
// K_THREAD_DEFINE(modem_recent_shutdown_worker, 512, shutdown_decrementer_worker, NULL, NULL, NULL,
K_THREAD_DEFINE(modem_recent_shutdown_worker, 1024, shutdown_decrementer_worker, NULL, NULL, NULL,
        K_LOWEST_APPLICATION_THREAD_PRIO, K_ESSENTIAL, 0);

static void on_modem_shutdown(void *ctx){
    LOG_WRN("modem shutting down. recent shutdown no: %d", k_sem_count_get(&modem_recent_shutdown_sem));
    k_sem_give(&modem_recent_shutdown_sem);
    if (k_sem_count_get(&modem_recent_shutdown_sem) >= MODEM_MAX_DAILY_SHUTDOWNS){
        LOG_ERR("resetting application due to %d modem shutdowns in last 24h!", MODEM_MAX_DAILY_SHUTDOWNS);
        k_panic();
    }
}
NRF_MODEM_LIB_ON_SHUTDOWN(shutdown_counter, on_modem_shutdown, NULL);
/* end modem reset behaviour */

/* sets a fallback dns*/
static void on_modem_init(int ret, void *ctx){
    struct nrf_in_addr dns;
    dns.s_addr = 0x08080808; // Google DNS, 8.8.8.8
    nrf_setdnsaddr(NRF_AF_INET, &dns, sizeof(struct nrf_in_addr));
}
NRF_MODEM_LIB_ON_INIT(dns_setter, on_modem_init, NULL);

int modem_init(void){
#if NCS_VERSION_NUMBER >= 0x20100
    int ret = nrf_modem_lib_init();
    if (ret != 0) {
        printk("Modem library initialization failed, error: %d\n", ret);
        return ret;
#else
    int ret = 0;
    if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
        /* Do nothing, modem is already configured and LTE connected. */
#endif
    }

    return ret;
}

int modem_shutdown(void){
    int ret = nrf_modem_lib_shutdown();
    if (ret != 0) {
        printk("Modem library shutdown failed, error: %d\n", ret);
        return ret;
    }
    return ret;
}

int slm_vbat(int* bat_mv){
    int ret = 0;
    char response[1024];

    ret = nrf_modem_at_cmd(response, sizeof(response), "AT%%XVBAT");
    if(ret == 0){
        char* start = strchr(response, ':')+1;
        char* end = strchr(start, '\n');
        *bat_mv = strtol(start, &end, 10);
    }
    return ret;
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
        uint8_t matches = sscanf(start, "(%hhu,%*[^,],%*[^,],\"%6[^\"]\",%hhu)", &o->status, o->mccmnc, &o->netact);
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
        modem_current_mccmnc(mccmnc_current);
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

    int lte, nbiot, gnss, preferred = -1;
    ret = nrf_modem_at_scanf("AT%XSYSTEMMODE?", "%%XSYSTEMMODE: %d,%d,%d,%d", &lte, &nbiot, &gnss, &preferred);
    if(ret == 4){LOG_INF("XSYSTEMMODE read ok");}
    else if (ret < 0){LOG_ERR("XSYSTEMMODE read error"); return ret;}
    else {LOG_ERR("XSYSTEMMODE scanf error"); return ret;}

    if (lte != 1 || nbiot != 0 || preferred != 1){
        ret = nrf_modem_at_printf("AT+CFUN=0");
        if(ret == 0){LOG_INF("CFUN off ok");}
        else if (ret < 0){LOG_ERR("CFUN off error"); return ret;}

        ret = nrf_modem_at_printf("AT%%XSYSTEMMODE=1,0,%d,1", gnss);
        if(ret == 0){LOG_INF("XSYSTEMMODE set ok");}
        else if (ret < 0){LOG_ERR("XSYSTEMMODE set error"); return ret;}
    }

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