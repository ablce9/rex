#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "buffer.h"
#include "region.h"

static __buffer_t *realloc_chain_buffer(__buffer_t *buf, size_t want) {
    region_t   *new_region;
    __buffer_t *new_buf;

    new_region = reallocate_region(buf->r, want);

    new_buf = new_region->data;

    new_buf->start = buf->start;
    new_buf->pos   = buf->pos;
    new_buf->end   = buf->pos + want;
    new_buf->r = new_region;
    new_buf->size += want;

    return new_buf;
}

static void cleanup(void *p) {
    void       *tmp;
    __buffer_t *buf;

    tmp = p;

    tmp += sizeof(region_t);
    buf = (__buffer_t *)tmp;
    free(buf->start);
}

region_t *create_chain_buffer(region_t *r, size_t size) {
    region_t   *new_region;
    __buffer_t *buf;

    new_region = ralloc(r, sizeof(__buffer_t));
    if (new_region == NULL) {
	return NULL;
    }

    buf = (__buffer_t *)new_region->data;

    buf->start = malloc(size);
    if (buf->start == NULL) {
	return NULL;
    }

    memset(buf->start, 0, size);

    buf->r          = new_region;
    buf->r->cleanup = cleanup;
    buf->pos        = buf->start;
    buf->end        = buf->start + size;
    buf->size       = size;

    new_region->data = buf;

    return new_region;
}

char *split_chain_buffer(__buffer_t *src_buf, size_t size) {
    size_t remained_space_size;

    remained_space_size = (src_buf->end - src_buf->pos);

    // Add for line terminator.
    size += 1;

    if (remained_space_size <= size) {
        printf("[debug] No space left for buffer, allocating new space: %ld bytes\n", size);
        src_buf = realloc_chain_buffer(src_buf, size);
    }

    src_buf->pos[size] = '\0';
    src_buf->pos += size;

    return src_buf->pos;
}
