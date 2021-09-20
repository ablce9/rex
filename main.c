#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <uv.h>

#include "buffer.h"


#define LF '\n'
#define CR '\r'
#define CRLF "\r\n"

#define MAX_HTTP_HEADER_LINE_BUFFER_SIZE 4096

typedef struct {
    char *header;
    char *body;
} http_response_payload_t;

typedef struct {
    char *key;
    void *value;
} __hash_t;

typedef struct {
    // todo: add http version and verb
    // char *start;
    __buffer_t *start;
    int start_length;
    __hash_t *meta[1024];
} http_request_header_t;

typedef struct {
    http_request_header_t *header;
    // todo: add body
} http_request_payload_t;

typedef struct {
    http_response_payload_t *response;
    http_request_payload_t *request;
} request_data_t;

static http_request_header_t *new_http_request_header(void);
static http_request_header_t *set_http_request_header(http_request_header_t *header, const char *header_buf, const int header_size);
static http_response_payload_t* new_http_response_payload(char *header, char *body);
static http_request_payload_t* new_http_request_payload(const char *request_buf, const int request_size);
static void set_http_request_header_start(http_request_header_t *header, const char *line_buf, const int line_length);
static __hash_t *parse_http_request_meta_header_line(const char *line_buf);
static void prepare_http_header(size_t content_length, uv_buf_t *buf);


static uv_loop_t *loop;

static void close_cb(uv_handle_t* handle) {
    if (handle->data) {
	__hash_t *hash;
	int i = 0;
	request_data_t *data = handle->data;
	http_response_payload_t *response = data->response;
	http_request_payload_t *request = data->request;

	while ((hash = request->header->meta[i])) {
	    free(hash->key);
	    free(hash->value);
	    free(hash);
	    ++i;
	}
	free(request->header->start->bytes);
	free(request->header->start);
	free(request->header);
	free(request);

	free(response->header);
	free(response->body);
	free(response);

	free(data);
    };
    free(handle);
}

static void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

static void format_string(char *string, size_t size, char *fmt, ...)
{
   va_list arg_ptr;

   va_start(arg_ptr, fmt);
   vsnprintf(string, size, fmt, arg_ptr);
   va_end(arg_ptr);
}

static char *build_http_header(size_t content_length) {
    char *fmt =								\
	"HTTP/1.1 200 OK\n"						\
	"Server: nginx\n" \
	"Content-Length: %zu\n" \
	"Date: Sun, 18 Apr 2021 04:13:15 GMT\n"				\
	"Content-Type: text/html; charset=UTF-8\n"			\
	CRLF;
    char buf[4049];

    memset(buf, 0, sizeof(buf));
    format_string(buf, sizeof(buf), fmt, content_length);

    char *header_buffer = malloc(sizeof(char*)*sizeof(buf));
    memcpy(header_buffer, buf, sizeof(buf));

    return header_buffer;
}

static void prepare_http_header(size_t content_length, uv_buf_t *buf) {
    char *header_buffer = build_http_header(content_length);

    buf->base = header_buffer;
    buf->len = strlen(header_buffer);
}

static int make_file_buffer(uv_buf_t *file_buffer) {
    const char *filename = "index.html";
    uv_fs_t stat_req;
    int result = uv_fs_stat(NULL, &stat_req, filename, NULL);
    if (result != 0) return 0;

    size_t file_size = ((uv_stat_t*)&stat_req.statbuf)->st_size;
    char *read_file_buffer = malloc(sizeof(char)*file_size);
    memset(read_file_buffer, 0, file_size);

    uv_fs_t open_req, read_req, close_req;
    uv_buf_t read_file_data = {
	.base = read_file_buffer,
	.len = file_size,
    };

    uv_fs_open(NULL, &open_req, filename, O_RDONLY, 0, NULL);
    uv_fs_read(NULL, &read_req, open_req.result, &read_file_data, 1, 0, NULL);
    uv_fs_close(NULL, &close_req, open_req.result, NULL);

    *file_buffer = read_file_data;
    return file_size;
}

static void write_cb(uv_write_t *request, int status) {
    if (status < 0) {
	fprintf(stderr, "write error %i\n", status);
    };
    free(request);
}

static uv_buf_t* new_http_iovec() {
    uv_buf_t* iovec = malloc(sizeof(uv_buf_t)*2);
    return iovec;
}

static http_response_payload_t *new_http_response_payload(char *header, char *body) {
    http_response_payload_t *http_payload = malloc(sizeof(http_response_payload_t));
    http_response_payload_t hrp = {
	.header = header,
	.body = body
    };
    *http_payload = hrp;
    return http_payload;
}

static http_request_payload_t *new_http_request_payload(const char *request_buf, const int request_size) {
    http_request_header_t *header = new_http_request_header();
    http_request_payload_t *request = calloc(1, sizeof(http_request_payload_t));

    set_http_request_header(header, request_buf, request_size);

    http_request_payload_t req = {
	.header = header
    };
    *request = req;

    return request;
}

static __hash_t *parse_http_request_meta_header_line(const char *line_buf) {
    __hash_t *hash = calloc(1, sizeof(__hash_t));
    char ch, key_buf[2048], value_buf[2048];
    int i = 0, key_len = 0, value_len = 0;

    for (i = 0; i < strlen(line_buf); i++) {
	ch = line_buf[i];

	key_buf[i] = ch;
	++key_len;
	if (ch == ':') {
	    hash->key = malloc(sizeof(char)*i);
	    key_buf[i] = '\0';
	    memcpy(hash->key, key_buf, i);
	}
    }

    for (i = key_len + 1; i < strlen(line_buf); i++) {
	ch = line_buf[i];
	if (ch != ' ') {
	    value_buf[value_len] = ch;
	    ++value_len;
	}
    }

    hash->value = malloc(sizeof(char)*value_len);
    value_buf[value_len] = '\0';
    memcpy(hash->value, (void*)value_buf, value_len);
    return hash;
}

static void set_http_request_header_start(http_request_header_t *header, const char *line_buf, const int line_length) {
    __buffer_t *buf = alloc_new_buffer(line_buf);
    header->start = buf;
}

static http_request_header_t *new_http_request_header(void) {
    http_request_header_t *header = calloc(1, sizeof(http_request_header_t));

    return header;
}

static http_request_header_t *set_http_request_header(http_request_header_t *header, const char *header_buf, const int header_size) {
    enum {
	start = 0,
	almost_done,
	done
    } state;

    char line[MAX_HTTP_HEADER_LINE_BUFFER_SIZE], ch;
    int i = 0, line_length = 0, hash_index = 0;
    __hash_t *hash;

    state = start;

    memset(line, 0, MAX_HTTP_HEADER_LINE_BUFFER_SIZE);

    // Parse http method and version
    for (i = 0; i < header_size; ++i) {

	ch = header_buf[i];

	switch (ch) {
	case CR:
	    state = almost_done;
	    break;
	case LF:
	    if (state == almost_done && line_length <= MAX_HTTP_HEADER_LINE_BUFFER_SIZE) {
		line[line_length] = '\0';
		set_http_request_header_start(header, line, line_length);
		state = done;
		break;
	    }
	default:
	    line[line_length] = ch;
	    ++line_length;
	}
	if (state == done) {
	    break;
	};
    }

    memset(line, 0, MAX_HTTP_HEADER_LINE_BUFFER_SIZE); // reset
    state = start;
    line_length = 0;
    int start_length = header->start->size + 1;

    // Parse http meta headers
    for (start_length = header->start_length + 1; start_length < header_size; ++start_length) {

	ch = header_buf[i];

	switch (ch) {
	case CR:
	    state = almost_done;
	    break;
	case LF:
	    if (state == almost_done && line_length <= MAX_HTTP_HEADER_LINE_BUFFER_SIZE) {
		hash = parse_http_request_meta_header_line(line);
		header->meta[hash_index] = hash;
		++hash_index;
	    };
	    memset(line, 0, MAX_HTTP_HEADER_LINE_BUFFER_SIZE); // reset
	    line_length = 0;
	    state = done;
	    break;
	default:
	    line[line_length] = ch;
	    ++line_length;
	}
    }
    return header;
}

static void read_cb(uv_stream_t* handle, ssize_t nread, const uv_buf_t* request_buf) {
    http_request_payload_t *request_payload = new_http_request_payload(request_buf->base, request_buf->len);

    uv_buf_t *response_vec = new_http_iovec();
    prepare_http_header(make_file_buffer(&response_vec[1]), &response_vec[0]);
    http_response_payload_t *response_payload = new_http_response_payload(response_vec[0].base, response_vec[1].base);

    request_data_t *data_p = calloc(1, sizeof(request_data_t));
    request_data_t data = {
	.response = response_payload,
	.request = request_payload
    };
    *data_p = data;
    handle->data = (void*)data_p;

    uv_write_t *writer = (uv_write_t*)malloc(sizeof(uv_write_t));
    uv_write(writer, handle, response_vec, 2, write_cb);

    uv_close((uv_handle_t*)handle, close_cb);

    free(request_buf->base);
    free(response_vec);
}

void on_new_connection(uv_stream_t* server, int status) {
    if (status < 0) {
	fprintf(stderr, "new connection error: %s\n", uv_strerror(status));
	return;
    }

    uv_tcp_t *client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));

    uv_tcp_init(loop, client);

    if (uv_accept(server, (uv_stream_t*)client) == 0) {
	uv_read_start((uv_stream_t*)client, alloc_buffer, read_cb);
    } else {
	printf("accept failed\n");
    }
}

int main() {
    uv_tcp_t server;

    loop = uv_default_loop();
    uv_tcp_init(loop, &server);

    struct sockaddr_in addr;

    uv_ip4_addr("0.0.0.0", 2020, &addr);
    uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);

    int r = uv_listen((uv_stream_t*) &server, 1024, on_new_connection);
    if (r) {
	fprintf(stderr, "Listen error %s\n", uv_strerror(r));
	return 1;
    }

    return uv_run(loop, UV_RUN_DEFAULT);
}
