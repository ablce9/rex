#ifndef __HTTP_H
#define __HTTP_H

#include <stdint.h>
#include <string.h>
#include <time.h>

#include "./buffer.h"
#include "./map.h"

#define HTTP_REQUEST_METHOD_GET  0x1
#define HTTP_REQUEST_METHOD_PUT  0x2
#define HTTP_REQUEST_METHOD_POST 0x4
#define HTTP_REQUEST_METHOD_HEAD 0x5
#define HTTP_REQUEST_METHOD_PATCH 0x6

#define MAX_HTTP_HEADER_LINE_BUFFER_SIZE 4096

#define REX_OK 0
#define REX_ERROR -1

#define str4comp(str, c0, c1, c2, c3) *(uint32_t *) str == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0)
#define str5comp(str, c0, c1, c2, c3, c4) \
    (*(uint32_t *) str == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0) && (str[4] == c4))

typedef intptr_t rex_int;

typedef struct {
    size_t    size;
    uint8_t   method;
    __map_t   *meta[1024];
    region_t  *r;
} http_header_t;

typedef struct {
    char *header;
    char *body;
} http_response_payload_t;

typedef struct {
    region_t      *r;
    http_header_t *header;
    // todo: add body
} http_request_payload_t;

rex_int parse_http_request_start(http_header_t *header, const char *line_buf);
rex_int parse_http_request(http_header_t *header, __buffer_t *header_buf, __buffer_t *chain_buf, __map_t *map);
http_header_t *new_http_request_header(region_t *r);
http_request_payload_t *new_http_request_payload(region_t *r);
char *make_http_response_header(http_header_t *h, char *chain_buf);

#endif //  __HTTP_H
