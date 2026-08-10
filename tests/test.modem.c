
#include "ncs_version.h"
#if NCS_VERSION_NUMBER < 0x020100
    #include <zephyr.h>
#endif
#include <modem/lte_lc.h>
#include <nrf_modem.h>
#include <nrf_modem_at.h>
#include <modem/nrf_modem_lib.h>
#include <hal/nrf_gpio.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <net/socket.h>
#include <net/http_client.h>

#include "../src/common.h"
#include "../src/modem.h"

LOG_MODULE_REGISTER(test_modem);

int test_automatic_network_selection(void){
    LOG_INF("TEST: AUTOMATIC NETWORK SELECTION");

    nrf_modem_lib_init();
    int ret = nrf_modem_at_printf("AT");
    if(ret == 0){LOG_INF("AT initialised");}
    else if (ret < 0){LOG_ERR("AT initialisation error"); return ret;}


   struct ftp_config_t ftp_cfg = {
        .mccmnc = "50503",
        .apn = "simbase"
   };

    modem_network_register(&ftp_cfg);


    return 0;
}


int test_modem_shutdown_callback(void){
    int ret;
    LOG_INF("TEST: MODEM SHUTDOWN CALLBACK");

    nrf_modem_lib_init();

    ret = nrf_modem_at_printf("AT");
    if(ret == 0){LOG_INF("AT initialised");}
    else if (ret < 0){LOG_ERR("AT initialisation error"); return ret;}

    LOG_INF("lib shutdown");
    nrf_modem_lib_shutdown();

    nrf_modem_lib_init();

    ret = nrf_modem_at_printf("AT");
    if(ret == 0){LOG_INF("AT initialised");}
    else if (ret < 0){LOG_ERR("AT initialisation error"); return ret;}

    LOG_INF("wrapper shutdown");
    modem_shutdown();

    return 0;
}

int test_modem_shutdowns_trigger_reset(void){
    int ret;
    LOG_INF("TEST: MODEM SHUTDOWNS TRIGGER RESET");

    for(size_t i = 0;;i++){
        nrf_modem_lib_init();

        ret = nrf_modem_at_printf("AT");
        if(ret == 0){LOG_INF("AT initialised");}
        else if (ret < 0){LOG_ERR("AT initialisation error"); return ret;}

        LOG_INF("shutdown nos. %d", i);
        nrf_modem_lib_shutdown();
    }


    return 0;
}

#define HTTP_TIMEOUT_MS     10000
#define HTTP_RECV_BUF_SIZE  1024

static uint8_t recv_buf[HTTP_RECV_BUF_SIZE];

struct http_result {
    int status;
    size_t bytes;
};

static int64_t time_ms(void) {
    return k_uptime_get();
}

static void log_elapsed(const char *name, int64_t start) {
    LOG_INF("%-24s %lld ms", name, k_uptime_get() - start);
}

static void response_cb(struct http_response *rsp,
                        enum http_final_call final_data,
                        void *user_data) {
    struct http_result *result = user_data;

    if (rsp->http_status_code) {
        result->status = rsp->http_status_code;
    }

    result->bytes += rsp->body_frag_len;
    if (rsp->body_frag_len > 0) {
        size_t len = MIN(rsp->body_frag_len, 24);
        char buf[25];

        memcpy(buf, rsp->body_frag_start, len);
        buf[len] = '\0';

        LOG_INF("HTTP body first %u bytes: %s", len, buf);
    }

    if (final_data == HTTP_DATA_FINAL) {
        LOG_INF("HTTP body received: %d bytes", result->bytes);
    }
}

int http_get_test(const char *host,
                  const char *path) {
    int ret;
    int sock;
    int64_t t;

    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };

    struct addrinfo *res;

    struct http_result result = {
        .status = 0,
        .bytes = 0,
    };

    /* ------------------------------------------------ */
    /* DNS */
    /* ------------------------------------------------ */
    t = time_ms();
    ret = getaddrinfo(host, "80", &hints, &res);
    log_elapsed("DNS lookup", t);
    if (ret) {
        LOG_ERR("getaddrinfo failed (%d)", ret);
        return -ENOENT;
    }

    /* ------------------------------------------------ */
    /* TCP connect */
    /* ------------------------------------------------ */

    sock = socket(res->ai_family,
                  res->ai_socktype,
                  IPPROTO_TCP);
    if (sock < 0) {
        freeaddrinfo(res);
        LOG_ERR("socket failed");
        return -errno;
    }

    t = time_ms();
    ret = connect(sock,
                  res->ai_addr,
                  res->ai_addrlen);
    log_elapsed("TCP connect", t);
    freeaddrinfo(res);
    if (ret < 0) {
        LOG_ERR("connect failed (%d)", errno);
        close(sock);
        return -errno;
    }

    /* ------------------------------------------------ */
    /* HTTP GET */
    /* ------------------------------------------------ */

    struct http_request req = {
        .method = HTTP_GET,
        .url = path,
        .host = host,
        .protocol = "HTTP/1.1",

        .response = response_cb,

        .recv_buf = recv_buf,
        .recv_buf_len = sizeof(recv_buf),
    };

    t = time_ms();
    ret = http_client_req(sock,
                          &req,
                          HTTP_TIMEOUT_MS,
                          &result);
    log_elapsed("HTTP GET", t);
    close(sock);
    if (ret < 0) {
        LOG_ERR("http_client_req failed (%d)", ret);
        return ret;
    }

    LOG_INF("HTTP status %d", result.status);
    return result.status;
}

struct ftp_config_t ftp_cfg = {
    .mccmnc = "50501",
    .apn = "simbase",
    .domain = "ftp.bosl.com.au",
};

int test_modem_psm(uint8_t psm, uint8_t deregister) {
    int ret;
    int tau;
    int active;
    int64_t t;

    LOG_INF("TEST: MODEM PSM");

    nrf_gpio_cfg_input(LED_FLASH_INBUILT_PIN, NRF_GPIO_PIN_PULLDOWN);

    NRF_UARTE0->ENABLE = 0;
    NRF_SPIM1->ENABLE = 0;
    NRF_TWIM2->ENABLE = 0;

    sdhc_mount();

    nrf_gpio_cfg_input(WKE_PIN, NRF_GPIO_PIN_PULLDOWN);
    nrf_gpio_cfg_input(SCCB_PDN, NRF_GPIO_PIN_PULLUP);

    /* ------------------------------------------------ */
    /* Modem init */
    /* ------------------------------------------------ */
    t = time_ms();
    ret = nrf_modem_lib_init();
    log_elapsed("Modem init", t);
    if (ret) {
        LOG_ERR("Modem init failed");
        return ret;
    }

    /* ------------------------------------------------ */
    /* AT */
    /* ------------------------------------------------ */
    t = time_ms();
    ret = nrf_modem_at_printf("AT");
    log_elapsed("AT", t);
    if (ret) {
        LOG_ERR("AT failed");
        return ret;
    }

    k_sleep(K_SECONDS(30));

    /* ------------------------------------------------ */
    /* Registration */
    /* ------------------------------------------------ */
    t = time_ms();
    ret = modem_network_register(&ftp_cfg);
    log_elapsed("Network registration", t);
    if (ret) {
        LOG_ERR("Registration failed");
        return ret;
    }

    lte_lc_psm_req(psm);

    char buffer[256];
    for (size_t i = 0;; i++) {
        int64_t wake_start = time_ms();

        LOG_INF("-----------------------------------------");
        LOG_INF("Wake cycle %d", i);
        led(1);

        /* Wait for registration */
        if (deregister && (i > 0)) {
            t = time_ms();
            ret = modem_network_register(&ftp_cfg);
            log_elapsed("Network re-registration", t);
            if (ret) {
                LOG_ERR("Registration failed");
                return ret;
            }
        } else {
            t = time_ms();
            modem_wait_registration(2000);
            log_elapsed("Wait registration", t);
        }

        /* CEREG */
        t = time_ms();
        nrf_modem_at_cmd(buffer, sizeof(buffer), "AT+CEREG?");
        log_elapsed("AT+CEREG?", t);
        printk("%s\n", buffer);

        /* CPSMS */
        t = time_ms();
        nrf_modem_at_cmd(buffer, sizeof(buffer), "AT+CPSMS?");
        log_elapsed("AT+CPSMS?", t);
        printk("%s\n", buffer);

        /* PSM */
        t = time_ms();
        ret = lte_lc_psm_get(&tau, &active);
        log_elapsed("PSM query", t);
        if (!ret) {
            LOG_INF("TAU=%d s Active=%d s", tau, active);
        }

        /* Entire network transaction */
        t = time_ms();
        ret = http_get_test(
                "bosl.com.au",
                "/IoT/AquaforBeech/scripts/ReadMe_v2.php?SiteName=SC.csv&Key=Temp");
        log_elapsed("Network total", t);

        if (ret >= 200 && ret < 300) {
            LOG_INF("HTTP GET OK");
        } else {
            LOG_ERR("HTTP GET failed (%d)", ret);
        }

        if(deregister){
            t = time_ms();
            ret = modem_network_deregister();
            log_elapsed("Network de-registration", t);
            if (ret) {
                LOG_ERR("deregistration failed");
                return ret;
            }
        }

        LOG_INF("Awake duration: %lld ms",
                time_ms() - wake_start);
        led(0);

        LOG_INF("Sleeping...");
        k_sleep(K_SECONDS(180));
    }

    return 0;
}