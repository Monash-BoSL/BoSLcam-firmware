#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <net/socket.h>


#define HTTP_RECV_BUF_SIZE 1024


struct http_endpoint {
    const char *host;
    uint16_t port;

    struct sockaddr_in addr;
    bool addr_valid;

    int sock;
    bool connected;

    uint8_t recv_buf[HTTP_RECV_BUF_SIZE];

    size_t recv_len;
    int status;
    bool truncated;
};

int http_endpoint_init_ip(struct http_endpoint *ep,
                          const char *ip,
                          uint16_t port);

int http_endpoint_init_host(struct http_endpoint *ep,
                            const char *host,
                            uint16_t port);

int http_get(struct http_endpoint *ep, const char *path);

void http_disconnect(struct http_endpoint *ep);

char *http_body(struct http_endpoint *ep);

/******************************************************************************/
/* HELPERS                                                                    */
/******************************************************************************/

int http_get_uint32(struct http_endpoint *ep,
                    const char *path,
                    uint32_t *value);