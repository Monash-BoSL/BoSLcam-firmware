
#include <net/socket.h>
#include <net/http_client.h>

#include "common.h"
#include "http.h"

LOG_MODULE_REGISTER(http);

#define HTTP_TIMEOUT_MS     10000
#define HTTP_RECV_BUF_SIZE  1024

static uint8_t recv_buf[HTTP_RECV_BUF_SIZE];

struct http_result {
    int status;
    size_t bytes;
};

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
    ret = getaddrinfo(host, "80", &hints, &res);
    if (ret) { LOG_ERR("getaddrinfo failed (%d)", ret); return -ENOENT; }

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

    ret = connect(sock,
                  res->ai_addr,
                  res->ai_addrlen);
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

    ret = http_client_req(sock,
                          &req,
                          HTTP_TIMEOUT_MS,
                          &result);
    close(sock);
    if (ret < 0) {
        LOG_ERR("http_client_req failed (%d)", ret);
        return ret;
    }

    LOG_INF("HTTP status %d", result.status);
    return result.status;
}