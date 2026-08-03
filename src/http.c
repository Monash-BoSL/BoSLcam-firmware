#include <errno.h>
#include <string.h>

#include <net/socket.h>
#include <net/http_client.h>

#include "common.h"
#include "http.h"

LOG_MODULE_REGISTER(http);

#define HTTP_TIMEOUT_MS 10000

static int resolve_endpoint(struct http_endpoint *ep) {
    if (ep->addr_valid) {return 0;}

    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };

    struct addrinfo *res;
    int ret = getaddrinfo(ep->host, NULL, &hints, &res);
    if (ret) {
        LOG_ERR("DNS failed (%d)", ret);
        return -ENOENT;
    }

    struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
    ep->addr = *addr;
    ep->addr.sin_port = htons(ep->port);
    ep->addr_valid = true;

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr->sin_addr, ip_str, sizeof(ip_str));
    LOG_INF("DNS resolution: %s -> %s", log_strdup(ep->host), log_strdup(ip_str));

    freeaddrinfo(res);
    return 0;
}

int http_endpoint_init_ip(struct http_endpoint *ep,
                          const char *ip,
                          uint16_t port) {
    memset(ep, 0, sizeof(*ep));

    ep->host = ip;
    ep->port = port;
    ep->addr.sin_family = AF_INET;
    ep->addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &ep->addr.sin_addr) != 1) {
        return -EINVAL;
    }
    ep->addr_valid = true;

    return 0;
}

int http_endpoint_init_host(struct http_endpoint *ep,
                            const char *host,
                            uint16_t port) {
    memset(ep, 0, sizeof(*ep));
    ep->host = host;
    ep->port = port;

    return 0;
}

static int open_socket(struct http_endpoint *ep) {
    int sock;

    int ret = resolve_endpoint(ep);
    if (ret) {return ret; }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {return -errno; }

    ret = connect(sock, (struct sockaddr *)&ep->addr, sizeof(ep->addr));
    if (ret < 0) {
        close(sock);
        return -errno;
    }

    return sock;
}

static void response_cb(struct http_response *rsp,
                        enum http_final_call final_data,
                        void *user_data) {
    struct http_endpoint *ep = user_data;

    ep->recv_len = rsp->data_len;
    ep->status = rsp->http_status_code;
    ep->truncated = rsp->data_len > sizeof(ep->recv_buf);
}

static int do_request(const int sock, struct http_endpoint *ep, const char *path) {
    struct http_request req = {
        .method = HTTP_GET,
        .url = path,
        .host = ep->host,
        .protocol = "HTTP/1.1",
        .response = response_cb,
        .recv_buf = ep->recv_buf,
        .recv_buf_len = sizeof(ep->recv_buf),
    };
    ep->recv_len = 0;

    return http_client_req(sock, &req, HTTP_TIMEOUT_MS, ep);
}

int http_get(struct http_endpoint *ep, const char *path) {
    int ret;
    int sock;

    memset(ep->recv_buf, 0, sizeof(ep->recv_buf));
    ep->recv_len = 0;

    sock = open_socket(ep);
    if (sock < 0) {return sock; }

    ret = do_request(sock, ep, path);

    close(sock);

    if (ret < 0) {
        LOG_ERR("HTTP request failed (%d)", ret);
        return ret;
    }

    LOG_INF("HTTP request complete");
    return 0;
}

char *http_body(struct http_endpoint *ep) {
    char *body;
    body = strstr((char *)ep->recv_buf, "\r\n\r\n");
    if (body) { return body + 4; }
    return (char *)ep->recv_buf;
}

/******************************************************************************/
/* HELPERS                                                                    */
/******************************************************************************/

int http_get_uint32(struct http_endpoint *ep,
                    const char *path,
                    uint32_t *value) {
    int ret;
    ret = http_get(ep, path);
    if (ret) {return ret; }

    if (ep->status < 200 || ep->status >= 300) {
        return -EIO;
    }
    if (ep->truncated) {
        return -EMSGSIZE;
    }

    char *body = http_body(ep);
    char *end;
    unsigned long parsed = strtoul(body, &end, 10);
    if (end == body) {return -EINVAL; }

    *value = (uint32_t)parsed;

    return 0;
}