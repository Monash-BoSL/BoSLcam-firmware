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

    freeaddrinfo(res);

    return 0;
}

int http_endpoint_init_ip(struct http_endpoint *ep,
                          const char *ip,
                          uint16_t port) {
    memset(ep, 0, sizeof(*ep));

    ep->host = ip;
    ep->port = port;
    ep->sock = -1;
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
    ep->sock = -1;

    return 0;
}

static int ensure_connected(struct http_endpoint *ep) {
    if (ep->connected){return 0; }

    int ret = resolve_endpoint(ep);
    if (ret) {return ret; }

    ep->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ep->sock < 0) {return -errno; }

    ret = connect(ep->sock, (struct sockaddr *)&ep->addr, sizeof(ep->addr));
    if (ret < 0) {
        close(ep->sock);
        ep->sock = -1;
        return -errno;
    }

    ep->connected = true;
    return 0;
}

void http_disconnect(struct http_endpoint *ep) {
    if (ep->connected) {
        close(ep->sock);
        ep->sock = -1;
        ep->connected = false;
    }
}

static void response_cb(struct http_response *rsp,
                        enum http_final_call final_data,
                        void *user_data) {
    struct http_endpoint *ep = user_data;

    ep->recv_len = rsp->data_len;
    ep->status = rsp->http_status_code;
    ep->truncated = rsp->data_len > sizeof(ep->recv_buf);
}

static int do_request(struct http_endpoint *ep, const char *path) {
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

    return http_client_req(ep->sock, &req, HTTP_TIMEOUT_MS, ep);
}

int http_get(struct http_endpoint *ep, const char *path) {
    int ret;

    memset(ep->recv_buf, 0, sizeof(ep->recv_buf));
    ep->recv_len = 0;

    ret = ensure_connected(ep);
    if (ret) {return ret; }

    ret = do_request(ep, path);
    /*
     * Server may have closed keep-alive connection.
     * Retry once.
     */
    if (ret < 0) {
        LOG_WRN("HTTP retry after reconnect");
        http_disconnect(ep);

        ret = ensure_connected(ep);
        if (ret) {return ret; }

        ret = do_request(ep, path);
    }

    if (ret < 0) {
        LOG_ERR("HTTP request failed (%d)", ret);
        http_disconnect(ep);
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