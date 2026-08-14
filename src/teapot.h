#ifndef STB_TEAPOT_H
#define STB_TEAPOT_H

// Usage:
//
// #define STB_TEAPOT_IMPLEMENTATION
// #include "stb_teapot.h"
// ... your code ...

// stb_teapot.h is a single-header C library that provides a simple HTTP server implementation.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS (1)
#endif

#ifdef _WIN32
#define TP_SIZE_T_FMT "%llu"
#define TP_INT_T_FMT "%ld"
#else
#define TP_SIZE_T_FMT "%zu"
#define TP_INT_T_FMT "%d"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _WINUSER_
#define _WINGDI_
#define _IMM_
#define _WINCON_
#include <windows.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
    typedef SOCKET stb_teapot_socket_t;
    int teapot_socket_ok(stb_teapot_socket_t s);
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
    typedef int stb_teapot_socket_t;
    int teapot_socket_ok(stb_teapot_socket_t s);
#endif

// Initial capacity of a dynamic array
#ifndef TP_DA_INIT_CAP
#define TP_DA_INIT_CAP 256
#endif

#ifndef TP_MAX_HEADER_NAME_LEN
#define TP_MAX_HEADER_NAME_LEN 256
#endif
#ifndef TP_MAX_HEADER_VALUE_LEN
#define TP_MAX_HEADER_VALUE_LEN 4096
#endif

#ifndef TEAPOT_MAX_BODY_SIZE
#define TEAPOT_MAX_BODY_SIZE (4u * 1024u * 1024u)
#endif
#ifndef TEAPOT_RECV_TIMEOUT_MS
#define TEAPOT_RECV_TIMEOUT_MS 5000
#endif

#ifdef __cplusplus
#define TP_DECLTYPE_CAST(T) (decltype(T))
#else
#define TP_DECLTYPE_CAST(T)
#endif // __cplusplus

#ifndef TP_ASSERT
#include <assert.h>
#define TP_ASSERT assert
#endif /* TP_ASSERT */

#ifndef TP_REALLOC
#include <stdlib.h>
#define TP_REALLOC realloc
#endif /* TP_REALLOC */

#ifndef TP_FREE
#include <stdlib.h>
#define TP_FREE free
#endif /* TP_FREE */

// =====================================================
// 🧩 Dynamic Array Macros (inspired by Nob)
// =====================================================
#define tp_da_free(da) TP_FREE((da).items)
#define tp_da_len(da) ((da).count)

#define tp_da_reserve(da, expected_capacity)                                                                            \
    do                                                                                                                  \
    {                                                                                                                   \
        if ((expected_capacity) > (da)->capacity)                                                                       \
        {                                                                                                               \
            if ((da)->capacity == 0)                                                                                    \
            {                                                                                                           \
                (da)->capacity = TP_DA_INIT_CAP;                                                                        \
            }                                                                                                           \
            while ((expected_capacity) > (da)->capacity)                                                                \
            {                                                                                                           \
                (da)->capacity *= 2;                                                                                    \
            }                                                                                                           \
            (da)->items = TP_DECLTYPE_CAST((da)->items) TP_REALLOC((da)->items, (da)->capacity * sizeof(*(da)->items)); \
            TP_ASSERT((da)->items != NULL && "Buy more RAM lol");                                                       \
        }                                                                                                               \
    } while (0)

// Append an item to a dynamic array
#define tp_da_append(da, item)                \
    do                                        \
    {                                         \
        tp_da_reserve((da), (da)->count + 1); \
        (da)->items[(da)->count++] = (item);  \
    } while (0)

// Append several items to a dynamic array
#define tp_da_append_many(da, new_items, new_items_count)                                           \
    do                                                                                              \
    {                                                                                               \
        size_t tp_da_new_items_count__ = (new_items_count);                                         \
        if (tp_da_new_items_count__ > 0)                                                            \
        {                                                                                           \
            tp_da_reserve((da), (da)->count + tp_da_new_items_count__);                             \
            memcpy((da)->items + (da)->count, (new_items),                                          \
                   tp_da_new_items_count__ * sizeof(*(da)->items));                                 \
            (da)->count += tp_da_new_items_count__;                                                 \
        }                                                                                           \
    } while (0)

    typedef struct
    {
        char *items;
        size_t count;
        size_t capacity;
    } tp_string_builder;

// Append a sized buffer to a string builder
#define tp_sb_append_buf(sb, buf, size) tp_da_append_many(sb, buf, size)

// Append a NULL-terminated string to a string builder
#define tp_sb_append_cstr(sb, cstr)  \
    do                               \
    {                                \
        const char *s = (cstr);      \
        size_t n = strlen(s);        \
        tp_da_append_many(sb, s, n); \
    } while (0)

    // Append a single NULL character at the end of a string builder. So then you can
// use it a NULL-terminated C string
#define tp_sb_append_null(sb) tp_da_append_many(sb, "", 1)

    int tp_sb_appendf(tp_string_builder *sb, const char *fmt, ...);

// Free the memory allocated by a string builder
#define tp_sb_free(sb) TP_FREE((sb).items)

    // =====================================================
    // 🌐 HTTP Core Types
    // =====================================================
    typedef enum
    {
        TEAPOT_GET,
        TEAPOT_POST,
        TEAPOT_PUT,
        TEAPOT_DELETE,
        TEAPOT_UNKNOWN
    } teapot_method;

    /* Common HTTP response status codes (for teapot_response.status) */
    typedef enum
    {
        TEAPOT_HTTP_OK = 200,
        TEAPOT_HTTP_CREATED = 201,
        TEAPOT_HTTP_BAD_REQUEST = 400,
        TEAPOT_HTTP_NOT_FOUND = 404,
        TEAPOT_HTTP_METHOD_NOT_ALLOWED = 405,
        TEAPOT_HTTP_UNSUPPORTED_MEDIA_TYPE = 415,
        TEAPOT_HTTP_INTERNAL_ERROR = 500,
    } teapot_http_status;

    typedef struct
    {
        tp_string_builder name;
        tp_string_builder value;
    } tp_header_line;

    typedef struct
    {
        tp_header_line *items;
        size_t count;
        size_t capacity;
    } tp_headers;

    typedef enum
    {
        TP_HEADER_NOT_FOUND = 0,
        TP_HEADER_FOUND = 1,
        TP_HEADER_MATCH = 2
    } tp_header_result;

    typedef struct
    {
        teapot_method method;
        tp_string_builder path;
        tp_string_builder body;
        tp_headers headers;
        size_t body_length;
    } teapot_request;

    typedef struct
    {
        int status;
        const char *content_type; /* NULL => "text/plain" */
        tp_string_builder body;
    } teapot_response;

    static inline void teapot_response_init(teapot_response *res, int status)
    {
        res->status = status;
        res->content_type = NULL;
        res->body.items = NULL;
        res->body.count = 0;
        res->body.capacity = 0;
    }

    static inline void teapot_response_write(teapot_response *res, const void *data, size_t len)
    {
        if (res == NULL || data == NULL || len == 0)
        {
            return;
        }

        tp_sb_append_buf(&res->body, data, len);
    }

    static inline void teapot_response_free(teapot_response *res)
    {
        tp_sb_free(res->body);
        res->body.items = NULL;
        res->body.count = 0;
        res->body.capacity = 0;
    }

    static inline void tp_headers_free(tp_headers *h)
    {
        if (h == NULL)
            return;
        for (size_t i = 0; i < h->count; ++i)
        {
            tp_sb_free(h->items[i].name);
            tp_sb_free(h->items[i].value);
        }
        TP_FREE(h->items);
        h->items = NULL;
        h->count = 0;
        h->capacity = 0;
    }

    /* Find a header value (case-insensitive). Returns pointer to the value string-builder, or NULL if not found */
    const tp_string_builder *tp_headers_get(const tp_headers *h, const char *name);

    // Check for existence and optionally match of a header. If found, fills o_header_line with the header line.
    tp_header_result tp_headers_check(const tp_headers *h, const char *name, const char *expected_value, tp_header_line *o_header_line);

    // =====================================================
    // 🚏 Routing and Server Types
    // =====================================================
    typedef teapot_response (*teapot_handler)(const teapot_request *);

    typedef struct
    {
        teapot_method method;
        const char *path;
        teapot_handler handler;
        int prefix; /* 1 = path prefix; route path must end in '/' */
    } teapot_route;

    typedef struct
    {
        int port;
        const teapot_route *routes;
        size_t route_count;
        const char *bind_host;
        int backlog;
        volatile sig_atomic_t stop;
        void *user;
        int max_conns;
    } teapot_server;

    // =====================================================
    // 🧠 API
    // =====================================================
    int teapot_listen(teapot_server *server);
    void teapot_request_stop(teapot_server *server);
    int teapot_listener_open(teapot_server *server, stb_teapot_socket_t *out_listen_sock);
    stb_teapot_socket_t teapot_listener_accept(stb_teapot_socket_t listen_sock);
    void teapot_close(stb_teapot_socket_t s);
    void teapot_listener_close(stb_teapot_socket_t listen_sock);
    int teapot_recv_request(stb_teapot_socket_t client, char *buffer, int bufsize, int *out_received);
    int teapot_send_response(stb_teapot_socket_t client, const teapot_response *resp);
    /* parse, complete body, route, send. Does NOT close client. */
    int teapot_serve_client(teapot_server *server, stb_teapot_socket_t client);
    /* teapot_serve_client + teapot_close. Takes ownership of client. */
    int teapot_handle_client_connection(teapot_server *server, stb_teapot_socket_t client);

