#ifndef __BUFFER_H
#define __BUFFER_H

#include <stdio.h>

#include "region.h"

typedef struct {
    size_t   size;
    char     *pos;
    char     *start;
    char     *end;
    region_t *r;
} rex_string_buffer_t;

#define BUFFER_CURRENT_SIZE(s, e, p) (s - ((e - p)/sizeof(char *)))

#define BUFFER_MOVE(p, b, s)				\
    do { \
	p = b; \
	b += s * sizeof(char *);		\
    } while(0);

region_t *create_chain_buffer(region_t *r, size_t size);
rex_string_buffer_t *alloc_new_buffer(const char *buffer);
rex_string_buffer_t *buffer_bytes_ncpy(const rex_string_buffer_t *src, const size_t size);
rex_string_buffer_t *alloc_string_buffer(rex_string_buffer_t *src_buf, size_t size);

#endif // __BUFFER_H
