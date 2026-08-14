/* GENERATED — do not edit. Source: src/ */
#ifndef STB_TEAPOT_H
#define STB_TEAPOT_H

#if !defined(_WIN32) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE 1
#endif

#define TEAPOT_WAIT_POLL 1
#define TEAPOT_WAIT_EPOLL 2
#define TEAPOT_WAIT_KQUEUE 3
#define TEAPOT_WAIT_WSAPOLL 4
#define TEAPOT_WAIT_WFMO 5

#if defined(TEAPOT_USE_WFMO)
#define TEAPOT_WAIT TEAPOT_WAIT_WFMO
#elif defined(TEAPOT_USE_POLL)
#define TEAPOT_WAIT TEAPOT_WAIT_POLL
#elif defined(TEAPOT_USE_EPOLL)
#define TEAPOT_WAIT TEAPOT_WAIT_EPOLL
#elif defined(TEAPOT_USE_KQUEUE)
#define TEAPOT_WAIT TEAPOT_WAIT_KQUEUE
#elif defined(TEAPOT_USE_WSAPOLL)
#define TEAPOT_WAIT TEAPOT_WAIT_WSAPOLL
#elif !defined(TEAPOT_WAIT)
#if defined(__linux__)
#define TEAPOT_WAIT TEAPOT_WAIT_EPOLL
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#define TEAPOT_WAIT TEAPOT_WAIT_KQUEUE
#elif defined(_WIN32)
#define TEAPOT_WAIT TEAPOT_WAIT_WSAPOLL
#else
#define TEAPOT_WAIT TEAPOT_WAIT_POLL
#endif
#endif

#if TEAPOT_WAIT < 1 || TEAPOT_WAIT > 5
#error "TEAPOT_WAIT backend unknown or unavailable on this OS"
#endif

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
        void *user;
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
    int teapot_run(teapot_server *server);
    void teapot_request_stop(teapot_server *server);
    int teapot_listener_open(teapot_server *server, stb_teapot_socket_t *out_listen_sock);
    stb_teapot_socket_t teapot_listener_accept(stb_teapot_socket_t listen_sock);
    void teapot_close(stb_teapot_socket_t s);
    void teapot_listener_close(stb_teapot_socket_t listen_sock);
    int teapot_recv_request(stb_teapot_socket_t client, char *buffer, int bufsize, int *out_received);
    int teapot_send_response(stb_teapot_socket_t client, const teapot_response *resp);
    teapot_response teapot_text(int status, const char *s);
    teapot_response teapot_json(int status, const char *json);
    teapot_response teapot_bytes(int status, const char *ctype, const void *p, size_t n);
    /* parse, complete body, route, send. Does NOT close client. */
    int teapot_serve_client(teapot_server *server, stb_teapot_socket_t client);
    /* teapot_serve_client + teapot_close. Takes ownership of client. */
    int teapot_handle_client_connection(teapot_server *server, stb_teapot_socket_t client);

#ifdef STB_TEAPOT_IMPLEMENTATION

#include <limits.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
    int teapot_socket_ok(stb_teapot_socket_t s)
    {
        return s != INVALID_SOCKET;
    }
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <arpa/inet.h>
#include <unistd.h>
    int teapot_socket_ok(stb_teapot_socket_t s)
    {
        return s >= 0;
    }
#endif

    static void teapot_init(void)
    {
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    }

    void teapot_close(stb_teapot_socket_t s)
    {
#ifdef _WIN32
        closesocket(s);
#else
        close(s);
#endif
    }

    /* One listener per process is the supported model on Windows (WSAStartup in listener_open). */
    void teapot_listener_close(stb_teapot_socket_t listen_sock)
    {
        teapot_close(listen_sock);
#ifdef _WIN32
        WSACleanup();
#endif
    }

    static int teapot_set_nonblock(stb_teapot_socket_t fd)
    {
#ifdef _WIN32
        u_long mode = 1;
        return ioctlsocket(fd, FIONBIO, &mode) == 0 ? 0 : -1;
#else
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0)
            return -1;
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0 ? 0 : -1;
#endif
    }

    static int teapot_would_block(void)
    {
#ifdef _WIN32
        int e = WSAGetLastError();
        return e == WSAEWOULDBLOCK;
#else
        return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
    }

    static uint64_t tp_now_ms(void)
    {
#ifdef _WIN32
        return (uint64_t)GetTickCount64();
#else
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
            return 0;
        return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull;
#endif
    }

    static int teapot_read(stb_teapot_socket_t s, char *buf, int len)
    {
        if (len <= 0)
        {
            return 0;
        }

#ifdef _WIN32
        return recv(s, buf, len, 0);
#else
        return (int)read(s, buf, (size_t)len);
#endif
    }

    static int teapot_write(stb_teapot_socket_t s, const char *buf, int len)
    {
        if (len <= 0)
            return 0;
#ifdef _WIN32
        return send(s, buf, len, 0);
#else
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
        return (int)send(s, buf, (size_t)len, MSG_NOSIGNAL);
#endif
    }

    static int teapot_write_all(stb_teapot_socket_t s, const char *buf, size_t len)
    {
        size_t total = 0;
        while (total < len)
        {
            size_t remaining = len - total;
            int chunk = remaining > (size_t)INT_MAX ? INT_MAX : (int)remaining;
            int n = teapot_write(s, buf + total, chunk);
            if (n <= 0)
                return -1;
            total += (size_t)n;
        }
        return 0;
    }

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

    static int tp_stricmp(const char *a, const char *b)
    {
        if (a == b)
        {
            return 0;
        }

        if (!a)
        {
            return b ? -1 : 0;
        }

        if (!b)
        {
            return 1;
        }

        while (*a && *b)
        {
            int ca = tolower((unsigned char)*a);
            int cb = tolower((unsigned char)*b);

            if (ca != cb)
            {
                return ca - cb;
            }

            ++a;
            ++b;
        }
        return tolower((unsigned char)*a) - tolower((unsigned char)*b);
    }

    /* helper: find header line by name (returns NULL if not found) */
    static const tp_header_line *tp_headers_find(const tp_headers *h, const char *name)
    {
        if (h == NULL || name == NULL)
            return NULL;
        for (size_t i = 0; i < h->count; ++i)
        {
            const char *hn = h->items[i].name.items ? h->items[i].name.items : "";
            if (tp_stricmp(hn, name) == 0)
                return &h->items[i];
        }
        return NULL;
    }

    typedef struct
    {
        const char *p;
        size_t n;
    } tp_span;

    static tp_span tp_span_trim(const char *s, size_t len)
    {
        while (len > 0 && isspace((unsigned char)*s))
        {
            ++s;
            --len;
        }
        while (len > 0 && isspace((unsigned char)s[len - 1]))
        {
            --len;
        }
        return (tp_span){s, len};
    }

    /* HTTP/1.1 subset: no obs-fold. Oversize names/values are rejected. */
    static int tp_parse_and_append_header_line(tp_headers *headers_parsed, const char *line, size_t linelen)
    {
        if (!headers_parsed || !line || linelen == 0)
        {
            return 0;
        }

        const char *colon = (const char *)memchr(line, ':', linelen);
        if (!colon)
        {
            return 0;
        }

        tp_span name = tp_span_trim(line, (size_t)(colon - line));
        tp_span value = tp_span_trim(colon + 1, (size_t)((line + linelen) - (colon + 1)));

        if (name.n == 0)
        {
            return 0;
        }
        if (name.n > (size_t)TP_MAX_HEADER_NAME_LEN || value.n > (size_t)TP_MAX_HEADER_VALUE_LEN)
        {
            return 0;
        }

        tp_header_line header_line = {0};
        tp_sb_append_buf(&header_line.name, name.p, name.n);
        tp_sb_append_null(&header_line.name);
        if (value.n > 0)
        {
            tp_sb_append_buf(&header_line.value, value.p, value.n);
            tp_sb_append_null(&header_line.value);
        }
        tp_da_append(headers_parsed, header_line);
        return 1;
    }

    static const char *tp_find_crlf(const char *buf, size_t n)
    {
        const char *end = buf + n;
        while (buf < end)
        {
            const char *p = (const char *)memchr(buf, '\r', (size_t)(end - buf));
            if (!p || p + 1 >= end)
                return NULL;
            if (p[1] == '\n')
                return p;
            buf = p + 1;
        }
        return NULL;
    }

    /* 1 = blank line ended the block, 0 = buffer ended, -1 = bad line. */
    static int tp_parse_header_block(tp_headers *h, const char *raw, size_t n, size_t *out_consumed)
    {
        size_t i = 0;
        while (i < n)
        {
            if (i + 1 < n && raw[i] == '\r' && raw[i + 1] == '\n')
            {
                *out_consumed = i + 2;
                return 1;
            }
            const char *eol = tp_find_crlf(raw + i, n - i);
            if (!eol)
                return -1;
            size_t linelen = (size_t)(eol - (raw + i));
            if (tp_parse_and_append_header_line(h, raw + i, linelen) != 1)
                return -1;
            i = (size_t)(eol - raw) + 2;
        }
        *out_consumed = i;
        return 0;
    }

    int tp_extract_header_keyval(tp_headers *headers_parsed, const char *raw_header, size_t header_size)
    {
        if (headers_parsed == NULL || raw_header == NULL)
            return -1;
        if (header_size == 0)
            return 0;

#ifdef TP_MAX_HEADER_TOTAL
        if (header_size > (size_t)TP_MAX_HEADER_TOTAL)
            header_size = (size_t)TP_MAX_HEADER_TOTAL;
#endif

        size_t consumed = 0;
        if (tp_parse_header_block(headers_parsed, raw_header, header_size, &consumed) < 0)
            return -1;
        (void)consumed;
        return 0;
    }

    const tp_string_builder *tp_headers_get(const tp_headers *h, const char *name)
    {
        const tp_header_line *hl = tp_headers_find(h, name);
        return hl ? &hl->value : NULL;
    }

    tp_header_result tp_headers_check(const tp_headers *h, const char *name, const char *expected_value, tp_header_line *o_header_line)
    {
        const tp_header_line *hl = tp_headers_find(h, name);
        if (!hl)
        {
            return TP_HEADER_NOT_FOUND;
        }
        if (o_header_line)
        {
            *o_header_line = *hl;
        }
        if (expected_value == NULL)
        {
            return TP_HEADER_FOUND;
        }
        const char *val = hl->value.items ? hl->value.items : "";
        return (strcmp(val, expected_value) == 0) ? TP_HEADER_MATCH : TP_HEADER_FOUND;
    }

    static const char *tp_skip_spaces(const char *p, const char *end)
    {
        while (p < end && *p == ' ')
            ++p;
        return p;
    }

    /* Route matching uses the request-target up to the first '?'. */
    static size_t tp_request_target_path_n(const char *target)
    {
        if (!target)
            return 0;
        const char *q = strchr(target, '?');
        return q ? (size_t)(q - target) : strlen(target);
    }

    static teapot_method parse_method(const char *s, size_t n)
    {
        if (n == 3 && memcmp(s, "GET", 3) == 0)
            return TEAPOT_GET;
        if (n == 4 && memcmp(s, "POST", 4) == 0)
            return TEAPOT_POST;
        if (n == 3 && memcmp(s, "PUT", 3) == 0)
            return TEAPOT_PUT;
        if (n == 6 && memcmp(s, "DELETE", 6) == 0)
            return TEAPOT_DELETE;
        return TEAPOT_UNKNOWN;
    }

    static int teapot_content_length_from_headers(const tp_headers *h, size_t *out_length)
    {
        *out_length = 0;
        int saw_cl = 0;
        unsigned long cl = 0;
        for (size_t i = 0; i < h->count; ++i)
        {
            const char *name = h->items[i].name.items ? h->items[i].name.items : "";
            if (tp_stricmp(name, "Transfer-Encoding") == 0)
                return -1;
            if (tp_stricmp(name, "Content-Length") != 0)
                continue;
            const char *val = h->items[i].value.items ? h->items[i].value.items : "";
            char *end = NULL;
            unsigned long n = strtoul(val, &end, 10);
            if (end == val)
                return -1;
            while (*end != '\0' && isspace((unsigned char)*end))
                ++end;
            if (*end != '\0' || n > (unsigned long)TEAPOT_MAX_BODY_SIZE)
                return -1;
            if (saw_cl && n != cl)
                return -1;
            saw_cl = 1;
            cl = n;
        }
        if (saw_cl)
            *out_length = (size_t)cl;
        return 0;
    }

    static void free_request(teapot_request *req)
    {
        tp_sb_free(req->path);
        tp_sb_free(req->body);
        tp_headers_free(&req->headers);
    }

    static int parse_request(char *buffer, size_t size, teapot_request *req, size_t *out_content_length)
    {
        if (buffer == NULL || size == 0 || req == NULL || out_content_length == NULL)
            return -1;

        req->path = (tp_string_builder){0};
        req->body = (tp_string_builder){0};
        req->headers = (tp_headers){0};
        req->body_length = 0;
        *out_content_length = 0;

        const char *line_end = tp_find_crlf(buffer, size);
        if (!line_end)
            return -1;

        size_t line_n = (size_t)(line_end - buffer);
        const char *sp1 = (const char *)memchr(buffer, ' ', line_n);
        if (!sp1)
            return -1;
        teapot_method method = parse_method(buffer, (size_t)(sp1 - buffer));
        if (method == TEAPOT_UNKNOWN)
            return -1;

        const char *path0 = tp_skip_spaces(sp1 + 1, line_end);
        if (path0 >= line_end)
            return -1;
        const char *sp2 = (const char *)memchr(path0, ' ', (size_t)(line_end - path0));
        if (!sp2)
            return -1;
        size_t path_n = (size_t)(sp2 - path0);
        if (path_n == 0)
            return -1;

        size_t line_end_off = (size_t)(line_end - buffer);
        if (line_end_off + 2 > size)
            return -1;
        size_t i = line_end_off + 2;
        size_t consumed = 0;
        if (tp_parse_header_block(&req->headers, buffer + i, size - i, &consumed) != 1)
            return -1;
        i += consumed;

        size_t content_length = 0;
        if (teapot_content_length_from_headers(&req->headers, &content_length) != 0)
            return -1;

        const char *body_start = buffer + i;
        size_t body_available = size > i ? size - i : 0;
        if (body_available > content_length)
            return -1;
        size_t to_append = content_length < body_available ? content_length : body_available;

        req->method = method;
        tp_sb_append_buf(&req->path, path0, path_n);
        tp_sb_append_null(&req->path);
        tp_sb_append_buf(&req->body, body_start, to_append);
        tp_sb_append_null(&req->body);
        req->body_length = to_append;
        *out_content_length = content_length;
        return 0;
    }

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

    static const char *teapot_status_to_str(int status)
    {
        switch (status)
        {
        case TEAPOT_HTTP_OK:
            return "OK";
        case TEAPOT_HTTP_CREATED:
            return "Created";
        case TEAPOT_HTTP_BAD_REQUEST:
            return "Bad Request";
        case TEAPOT_HTTP_NOT_FOUND:
            return "Not Found";
        case TEAPOT_HTTP_METHOD_NOT_ALLOWED:
            return "Method Not Allowed";
        case TEAPOT_HTTP_UNSUPPORTED_MEDIA_TYPE:
            return "Unsupported Media Type";
        case TEAPOT_HTTP_INTERNAL_ERROR:
            return "Internal Server Error";
        default:
            return "Unknown";
        }
    }

    int tp_sb_appendf(tp_string_builder *sb, const char *fmt, ...)
    {
        va_list args;

        va_start(args, fmt);
        int n = vsnprintf(NULL, 0, fmt, args);
        va_end(args);

        // NOTE: the new_capacity needs to be +1 because of the null terminator.
        // However, further below we increase sb->count by n, not n + 1.
        // This is because we don't want the sb to include the null terminator. The user can always sb_append_null() if they want it
        tp_da_reserve(sb, sb->count + (size_t)(n + 1));
        char *dest = sb->items + sb->count;
        va_start(args, fmt);
        vsnprintf(dest, (size_t)(n + 1), fmt, args);
        va_end(args);

        sb->count += (size_t)n;

        return n;
    }

    static int teapot_format_response(tp_string_builder *out, const teapot_response *resp)
    {
        if (!out || !resp)
            return -1;

        const char *ct = (resp->content_type != NULL) ? resp->content_type : "text/plain";
        if (strpbrk(ct, "\r\n") != NULL)
            return -1;

        tp_sb_appendf(out,
                      "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: " TP_SIZE_T_FMT "\r\nConnection: close\r\n\r\n",
                      resp->status, teapot_status_to_str(resp->status), ct, tp_da_len(resp->body));
        if (resp->body.count > 0)
            tp_sb_append_buf(out, resp->body.items, resp->body.count);
        return 0;
    }

    teapot_response teapot_text(int status, const char *s)
    {
        teapot_response r;
        teapot_response_init(&r, status);
        r.content_type = "text/plain";
        if (s)
            teapot_response_write(&r, s, strlen(s));
        return r;
    }

    teapot_response teapot_json(int status, const char *json)
    {
        teapot_response r;
        teapot_response_init(&r, status);
        r.content_type = "application/json";
        if (json)
            teapot_response_write(&r, json, strlen(json));
        return r;
    }

    teapot_response teapot_bytes(int status, const char *ctype, const void *p, size_t n)
    {
        teapot_response r;
        teapot_response_init(&r, status);
        r.content_type = ctype ? ctype : "text/plain";
        if (p && n > 0)
            teapot_response_write(&r, p, n);
        return r;
    }

    static teapot_handler teapot_find_handler(teapot_server *server, teapot_request *req)
    {
        const char *target = req->path.items ? req->path.items : "";
        size_t path_n = tp_request_target_path_n(target);
        for (size_t i = 0; i < server->route_count; i++)
        {
            const teapot_route *r = &server->routes[i];
            if (r->method != req->method)
                continue;
            size_t n = strlen(r->path);
            if (r->prefix)
            {
                if (n > 0 && r->path[n - 1] == '/' && path_n >= n && memcmp(r->path, target, n) == 0)
                    return r->handler;
            }
            else if (n == path_n && memcmp(r->path, target, n) == 0)
            {
                return r->handler;
            }
        }
        return NULL;
    }

    int teapot_recv_request(stb_teapot_socket_t client, char *buffer, int bufsize, int *out_received)
    {
        if (!teapot_socket_ok((stb_teapot_socket_t)client) || !buffer || bufsize <= 0)
        {
            return -1;
        }

        int received = teapot_read((stb_teapot_socket_t)client, buffer, bufsize - 1);
        if (received <= 0)
        {
            if (out_received)
            {
                *out_received = received;
            }
            return -1;
        }

        buffer[received] = '\0';
        if (out_received)
        {
            *out_received = received;
        }
        return 0;
    }

    int teapot_send_response(stb_teapot_socket_t client, const teapot_response *resp)
    {
        if (!teapot_socket_ok(client) || !resp)
            return -1;

        tp_string_builder out = {0};
        if (teapot_format_response(&out, resp) != 0)
        {
            tp_sb_free(out);
            return -1;
        }

        int rc = teapot_write_all(client, out.items, out.count);
        tp_sb_free(out);
        return rc;
    }

    int teapot_handle_client_connection(teapot_server *server, stb_teapot_socket_t client)
    {
        int rc = teapot_serve_client(server, client);
        teapot_close(client);
        return rc;
    }

#include <limits.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#endif

#ifndef TEAPOT_CONN_BUF
#define TEAPOT_CONN_BUF 8192
#endif

#ifndef TEAPOT_MAX_CONNS
#define TEAPOT_MAX_CONNS 128
#endif

    typedef enum
    {
        TEAPOT_IO_NEED_READ = 1,
        TEAPOT_IO_NEED_WRITE = 2,
        TEAPOT_IO_DONE = 0,
        TEAPOT_IO_ERROR = -1,
    } teapot_io;

    typedef enum
    {
        TEAPOT_CONN_READ_HEAD,
        TEAPOT_CONN_READ_BODY,
        TEAPOT_CONN_WRITE_RESP,
        TEAPOT_CONN_DONE,
    } teapot_conn_phase;

    typedef struct teapot_conn
    {
        teapot_server *server;
        stb_teapot_socket_t fd;
        teapot_conn_phase phase;
        char in[TEAPOT_CONN_BUF];
        size_t in_len;
        size_t header_end; /* index of '\r' of the blank line; 0 until seen */
        size_t body_need;
        size_t body_got;
        teapot_request req;
        teapot_response res;
        tp_string_builder out;
        size_t out_sent;
        uint64_t deadline_ms; /* set at init; does not reset */
        int failed;
        int slot_used;
    } teapot_conn;

    static size_t teapot_find_header_end(const char *buf, size_t n)
    {
        if (n < 4)
            return (size_t)-1;
        for (size_t i = 0; i + 3 < n; ++i)
        {
            if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n')
                return i;
        }
        return (size_t)-1;
    }

    static teapot_io teapot_conn_begin_400(teapot_conn *c)
    {
        teapot_response_free(&c->res);
        teapot_response_init(&c->res, TEAPOT_HTTP_BAD_REQUEST);
        tp_sb_appendf(&c->res.body, "400 Bad Request\n");
        tp_sb_free(c->out);
        c->out = (tp_string_builder){0};
        c->out_sent = 0;
        if (teapot_format_response(&c->out, &c->res) != 0)
        {
            c->failed = 1;
            c->phase = TEAPOT_CONN_DONE;
            return TEAPOT_IO_ERROR;
        }
        c->failed = 1;
        c->phase = TEAPOT_CONN_WRITE_RESP;
        return TEAPOT_IO_NEED_WRITE;
    }

    static teapot_io teapot_conn_dispatch(teapot_conn *c)
    {
        c->req.user = c->server->user;
        teapot_handler handler = teapot_find_handler(c->server, &c->req);
        teapot_response_free(&c->res);
        teapot_response_init(&c->res, TEAPOT_HTTP_OK);
        if (handler)
            c->res = handler(&c->req);
        else
        {
            c->res.status = TEAPOT_HTTP_NOT_FOUND;
            tp_sb_appendf(&c->res.body, "404 Not Found\n");
        }

        tp_sb_free(c->out);
        c->out = (tp_string_builder){0};
        c->out_sent = 0;
        if (teapot_format_response(&c->out, &c->res) != 0)
        {
            c->failed = 1;
            c->phase = TEAPOT_CONN_DONE;
            return TEAPOT_IO_ERROR;
        }
        c->failed = 0;
        c->phase = TEAPOT_CONN_WRITE_RESP;
        return TEAPOT_IO_NEED_WRITE;
    }

    void teapot_conn_init(teapot_conn *c, teapot_server *server, stb_teapot_socket_t fd)
    {
        memset(c, 0, sizeof(*c));
        c->server = server;
        c->fd = fd;
        c->phase = TEAPOT_CONN_READ_HEAD;
#if TEAPOT_RECV_TIMEOUT_MS > 0
        c->deadline_ms = tp_now_ms() + (uint64_t)TEAPOT_RECV_TIMEOUT_MS;
#else
        c->deadline_ms = 0;
#endif
    }

    void teapot_conn_free(teapot_conn *c)
    {
        if (!c)
            return;
        free_request(&c->req);
        c->req.path = (tp_string_builder){0};
        c->req.body = (tp_string_builder){0};
        teapot_response_free(&c->res);
        tp_sb_free(c->out);
        c->out = (tp_string_builder){0};
    }

    static teapot_io teapot_conn_read_head(teapot_conn *c)
    {
        if (c->in_len == (size_t)TEAPOT_CONN_BUF)
            return teapot_conn_begin_400(c);

        int space = (int)((size_t)TEAPOT_CONN_BUF - c->in_len);
        int n = (int)recv(c->fd, c->in + c->in_len, (size_t)space, 0);
        if (n < 0)
        {
            if (teapot_would_block())
                return TEAPOT_IO_NEED_READ;
            c->failed = 1;
            c->phase = TEAPOT_CONN_DONE;
            return TEAPOT_IO_ERROR;
        }
        if (n == 0)
            return teapot_conn_begin_400(c);

        c->in_len += (size_t)n;
        size_t hend = teapot_find_header_end(c->in, c->in_len);
        if (hend == (size_t)-1)
        {
            if (c->in_len == (size_t)TEAPOT_CONN_BUF)
                return teapot_conn_begin_400(c);
            return TEAPOT_IO_NEED_READ;
        }

        c->header_end = hend;
        free_request(&c->req);
        c->req = (teapot_request){0};
        size_t content_length = 0;
        if (parse_request(c->in, c->in_len, &c->req, &content_length) < 0)
            return teapot_conn_begin_400(c);

        c->body_need = content_length;
        c->body_got = c->req.body_length;
        if (c->body_need > (size_t)TEAPOT_MAX_BODY_SIZE)
            return teapot_conn_begin_400(c);

        if (c->body_got >= c->body_need)
        {
            if (c->req.body.count == c->req.body_length)
                tp_sb_append_null(&c->req.body);
            return teapot_conn_dispatch(c);
        }

        c->phase = TEAPOT_CONN_READ_BODY;
        return TEAPOT_IO_NEED_READ;
    }

    static teapot_io teapot_conn_read_body(teapot_conn *c)
    {
        if (c->body_need > (size_t)TEAPOT_MAX_BODY_SIZE)
            return teapot_conn_begin_400(c);

        char bounce[4096];
        size_t remaining = c->body_need - c->body_got;
        size_t to_read = remaining > sizeof(bounce) ? sizeof(bounce) : remaining;
        int n = (int)recv(c->fd, bounce, to_read, 0);
        if (n < 0)
        {
            if (teapot_would_block())
                return TEAPOT_IO_NEED_READ;
            c->failed = 1;
            c->phase = TEAPOT_CONN_DONE;
            return TEAPOT_IO_ERROR;
        }
        if (n == 0)
            return teapot_conn_begin_400(c);

        c->req.body.count = c->req.body_length;
        tp_sb_append_buf(&c->req.body, bounce, (size_t)n);
        c->req.body_length += (size_t)n;
        c->body_got = c->req.body_length;

        if (c->body_got < c->body_need)
            return TEAPOT_IO_NEED_READ;

        tp_sb_append_null(&c->req.body);
        return teapot_conn_dispatch(c);
    }

    static teapot_io teapot_conn_write_resp(teapot_conn *c)
    {
        if (c->out_sent >= c->out.count)
        {
            c->phase = TEAPOT_CONN_DONE;
            return TEAPOT_IO_DONE;
        }

        size_t remaining = c->out.count - c->out_sent;
        int chunk = remaining > (size_t)INT_MAX ? INT_MAX : (int)remaining;
#ifdef _WIN32
        int n = send(c->fd, c->out.items + c->out_sent, chunk, 0);
#else
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
        int n = (int)send(c->fd, c->out.items + c->out_sent, (size_t)chunk, MSG_NOSIGNAL);
#endif
        if (n < 0)
        {
            if (teapot_would_block())
                return TEAPOT_IO_NEED_WRITE;
            c->failed = 1;
            c->phase = TEAPOT_CONN_DONE;
            return TEAPOT_IO_ERROR;
        }
        if (n == 0)
        {
            c->failed = 1;
            c->phase = TEAPOT_CONN_DONE;
            return TEAPOT_IO_ERROR;
        }
        c->out_sent += (size_t)n;
        if (c->out_sent >= c->out.count)
        {
            c->phase = TEAPOT_CONN_DONE;
            return TEAPOT_IO_DONE;
        }
        return TEAPOT_IO_NEED_WRITE;
    }

    teapot_io teapot_conn_step(teapot_conn *c)
    {
        if (!c || !c->server || !teapot_socket_ok(c->fd))
            return TEAPOT_IO_ERROR;

        switch (c->phase)
        {
        case TEAPOT_CONN_READ_HEAD:
            return teapot_conn_read_head(c);
        case TEAPOT_CONN_READ_BODY:
            return teapot_conn_read_body(c);
        case TEAPOT_CONN_WRITE_RESP:
            return teapot_conn_write_resp(c);
        case TEAPOT_CONN_DONE:
            return c->failed ? TEAPOT_IO_ERROR : TEAPOT_IO_DONE;
        default:
            c->failed = 1;
            return TEAPOT_IO_ERROR;
        }
    }

    int teapot_serve_client(teapot_server *server, stb_teapot_socket_t client)
    {
        if (!server || !teapot_socket_ok(client))
            return -1;
        if (teapot_set_nonblock(client) != 0)
            return -1;

        teapot_conn c;
        teapot_conn_init(&c, server, client);
        int rc = -1;

        for (;;)
        {
            teapot_io io = teapot_conn_step(&c);
            if (io == TEAPOT_IO_DONE)
            {
                rc = c.failed ? -1 : 0;
                break;
            }
            if (io == TEAPOT_IO_ERROR)
            {
                rc = -1;
                break;
            }

            int timeout_ms = 250;
#if TEAPOT_RECV_TIMEOUT_MS > 0
            if (c.deadline_ms != 0 &&
                (c.phase == TEAPOT_CONN_READ_HEAD || c.phase == TEAPOT_CONN_READ_BODY))
            {
                uint64_t now = tp_now_ms();
                if (now >= c.deadline_ms)
                {
                    teapot_conn_free(&c);
                    return -1;
                }
                uint64_t rem = c.deadline_ms - now;
                if (rem < (uint64_t)timeout_ms)
                    timeout_ms = (int)rem;
                if (timeout_ms < 1)
                    timeout_ms = 1;
            }
#endif

#ifdef _WIN32
            WSAPOLLFD pfd;
            pfd.fd = client;
            pfd.events = (io == TEAPOT_IO_NEED_READ) ? POLLIN : POLLOUT;
            pfd.revents = 0;
            int pr = WSAPoll(&pfd, 1, timeout_ms);
#else
            struct pollfd pfd;
            pfd.fd = client;
            pfd.events = (io == TEAPOT_IO_NEED_READ) ? POLLIN : POLLOUT;
            pfd.revents = 0;
            int pr = poll(&pfd, 1, timeout_ms);
#endif
            if (server->stop)
            {
                teapot_conn_free(&c);
                return -1;
            }
            if (pr < 0)
            {
#ifdef _WIN32
                teapot_conn_free(&c);
                return -1;
#else
                if (errno == EINTR)
                    continue;
                teapot_conn_free(&c);
                return -1;
#endif
            }
            if (pr == 0)
            {
#if TEAPOT_RECV_TIMEOUT_MS > 0
                if (c.deadline_ms != 0 &&
                    (c.phase == TEAPOT_CONN_READ_HEAD || c.phase == TEAPOT_CONN_READ_BODY) &&
                    tp_now_ms() >= c.deadline_ms)
                {
                    teapot_conn_free(&c);
                    return -1;
                }
#endif
                continue;
            }
        }

        teapot_conn_free(&c);
        return rc;
    }

#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#endif

    void teapot_request_stop(teapot_server *server)
    {
        server->stop = 1;
    }

    static int teapot_wait_listen(stb_teapot_socket_t listen_sock, int timeout_ms)
    {
#ifdef _WIN32
        WSAPOLLFD pfd;
        pfd.fd = listen_sock;
        pfd.events = POLLIN;
        pfd.revents = 0;
        return WSAPoll(&pfd, 1, timeout_ms);
#else
        struct pollfd pfd;
        pfd.fd = listen_sock;
        pfd.events = POLLIN;
        pfd.revents = 0;
        int n;
        do
        {
            n = poll(&pfd, 1, timeout_ms);
        } while (n < 0 && errno == EINTR);
        return n;
#endif
    }

    int teapot_listener_open(teapot_server *server, stb_teapot_socket_t *out_listen_sock)
    {
        if (!server || !out_listen_sock)
            return -1;

        teapot_init();

        stb_teapot_socket_t s = socket(AF_INET, SOCK_STREAM, 0);
        if (!teapot_socket_ok(s))
        {
            perror("socket");
            return -1;
        }

        int yes = 1;
        (void)setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons((uint16_t)server->port);
        if (server->bind_host == NULL)
        {
            addr.sin_addr.s_addr = INADDR_ANY;
        }
        else if (inet_pton(AF_INET, server->bind_host, &addr.sin_addr) != 1)
        {
            teapot_close(s);
            return -1;
        }

        if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            perror("bind");
            teapot_close(s);
            return -1;
        }

        {
            struct sockaddr_in bound = {0};
            socklen_t blen = sizeof(bound);
            if (getsockname(s, (struct sockaddr *)&bound, &blen) < 0)
            {
                perror("getsockname");
                teapot_close(s);
                return -1;
            }
            server->port = (int)ntohs(bound.sin_port);
        }

        int backlog = server->backlog ? server->backlog : 8;
        if (listen(s, backlog) < 0)
        {
            perror("listen");
            teapot_close(s);
            return -1;
        }

        *out_listen_sock = (stb_teapot_socket_t)s;
        return 0;
    }

    stb_teapot_socket_t teapot_listener_accept(stb_teapot_socket_t listen_sock)
    {
        if (!teapot_socket_ok((stb_teapot_socket_t)listen_sock))
        {
            return (stb_teapot_socket_t)-1;
        }

        stb_teapot_socket_t client = accept((stb_teapot_socket_t)listen_sock, NULL, NULL);
        if (!teapot_socket_ok(client))
        {
            return (stb_teapot_socket_t)-1;
        }

        return (stb_teapot_socket_t)client;
    }

    int teapot_listen(teapot_server *server)
    {
        if (!server)
            return 1;

        stb_teapot_socket_t listen_sock;
        if (teapot_listener_open(server, &listen_sock) < 0)
            return 1;

        while (!server->stop)
        {
            int n = teapot_wait_listen(listen_sock, 250);
            if (server->stop)
                break;
            if (n <= 0)
                continue;
            stb_teapot_socket_t client = teapot_listener_accept(listen_sock);
            if (!teapot_socket_ok(client))
                continue;
            teapot_handle_client_connection(server, client);
        }

        teapot_listener_close(listen_sock);
        return 0;
    }

#if TEAPOT_WAIT == TEAPOT_WAIT_POLL
#ifndef _WIN32
#include <poll.h>

#define TEAPOT_WAIT_IN 1
#define TEAPOT_WAIT_OUT 2
#define TP_PE(e) ((short)(((e) & TEAPOT_WAIT_IN ? POLLIN : 0) | ((e) & TEAPOT_WAIT_OUT ? POLLOUT : 0)))

typedef struct { void *udata; int events; } tp_wait_event;
typedef struct { struct pollfd *pfds; void **udata; int count; int cap; } tp_wait;

static int tp_wait_find(tp_wait *w, stb_teapot_socket_t fd)
{
    int i;
    for (i = 0; i < w->count; ++i)
        if (w->pfds[i].fd == fd) return i;
    return -1;
}

int tp_wait_create(tp_wait *w) { memset(w, 0, sizeof(*w)); return 0; }

int tp_wait_add(tp_wait *w, stb_teapot_socket_t fd, int events, void *udata)
{
    if (w->count == w->cap) {
        int cap = w->cap ? w->cap * 2 : 16;
        struct pollfd *pfds = TP_REALLOC(w->pfds, (size_t)cap * sizeof(*pfds));
        void **ud;
        if (!pfds) return -1;
        w->pfds = pfds;
        ud = TP_REALLOC(w->udata, (size_t)cap * sizeof(*ud));
        if (!ud) return -1;
        w->udata = ud;
        w->cap = cap;
    }
    w->pfds[w->count] = (struct pollfd){.fd = fd, .events = TP_PE(events)};
    w->udata[w->count++] = udata;
    return 0;
}

int tp_wait_mod(tp_wait *w, stb_teapot_socket_t fd, int events, void *udata)
{
    int i = tp_wait_find(w, fd);
    if (i < 0) return -1;
    w->pfds[i].events = TP_PE(events);
    w->udata[i] = udata;
    return 0;
}

int tp_wait_del(tp_wait *w, stb_teapot_socket_t fd)
{
    int i = tp_wait_find(w, fd);
    if (i < 0) return -1;
    w->count--;
    if (i != w->count) { w->pfds[i] = w->pfds[w->count]; w->udata[i] = w->udata[w->count]; }
    return 0;
}

int tp_wait_wait(tp_wait *w, int timeout_ms, tp_wait_event *out, int max_out)
{
    int n, i, k = 0;
    do n = poll(w->pfds, (nfds_t)w->count, timeout_ms); while (n < 0 && errno == EINTR);
    if (n <= 0) return n;
    for (i = 0; i < w->count && k < max_out; ++i) {
        short re = w->pfds[i].revents;
        int events = 0;
        if (re & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) events |= TEAPOT_WAIT_IN;
        if (re & POLLOUT) events |= TEAPOT_WAIT_OUT;
        if (!events) continue;
        out[k].udata = w->udata[i];
        out[k++].events = events;
    }
    return k;
}

void tp_wait_destroy(tp_wait *w)
{
    TP_FREE(w->pfds);
    TP_FREE(w->udata);
    memset(w, 0, sizeof(*w));
}
#define TP_WAIT_READY 1
#endif
#endif

#if TEAPOT_WAIT == TEAPOT_WAIT_EPOLL
#ifndef __linux__
#error "TEAPOT_WAIT_EPOLL requires Linux"
#else
#include <sys/epoll.h>

#define TEAPOT_WAIT_IN 1
#define TEAPOT_WAIT_OUT 2
#define TP_EE(e) (((e) & TEAPOT_WAIT_IN ? EPOLLIN : 0u) | ((e) & TEAPOT_WAIT_OUT ? EPOLLOUT : 0u))

typedef struct { void *udata; int events; } tp_wait_event;
typedef struct { int epfd; } tp_wait;

int tp_wait_create(tp_wait *w) { w->epfd = epoll_create1(0); return w->epfd < 0 ? -1 : 0; }

int tp_wait_add(tp_wait *w, stb_teapot_socket_t fd, int events, void *udata)
{
    struct epoll_event ev = {.events = TP_EE(events), .data.ptr = udata};
    return epoll_ctl(w->epfd, EPOLL_CTL_ADD, fd, &ev) == 0 ? 0 : -1;
}

int tp_wait_mod(tp_wait *w, stb_teapot_socket_t fd, int events, void *udata)
{
    struct epoll_event ev = {.events = TP_EE(events), .data.ptr = udata};
    return epoll_ctl(w->epfd, EPOLL_CTL_MOD, fd, &ev) == 0 ? 0 : -1;
}

int tp_wait_del(tp_wait *w, stb_teapot_socket_t fd)
{
    return epoll_ctl(w->epfd, EPOLL_CTL_DEL, fd, NULL) == 0 ? 0 : -1;
}

int tp_wait_wait(tp_wait *w, int timeout_ms, tp_wait_event *out, int max_out)
{
    struct epoll_event buf[64];
    int lim = max_out < 64 ? max_out : 64, n, i, k = 0;
    do n = epoll_wait(w->epfd, buf, lim, timeout_ms); while (n < 0 && errno == EINTR);
    if (n <= 0) return n;
    for (i = 0; i < n; ++i) {
        uint32_t re = buf[i].events;
        int events = 0;
        if (re & (EPOLLIN | EPOLLHUP | EPOLLERR)) events |= TEAPOT_WAIT_IN;
        if (re & EPOLLOUT) events |= TEAPOT_WAIT_OUT;
        if (!events) continue;
        out[k].udata = buf[i].data.ptr;
        out[k++].events = events;
    }
    return k;
}

void tp_wait_destroy(tp_wait *w)
{
    if (w->epfd >= 0) close(w->epfd);
    memset(w, 0, sizeof(*w));
    w->epfd = -1;
}
#define TP_WAIT_READY 1
#endif
#endif

#if TEAPOT_WAIT == TEAPOT_WAIT_KQUEUE
#if !(defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__))
#error "TEAPOT_WAIT_KQUEUE requires Apple or BSD"
#else
#include <errno.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

#define TEAPOT_WAIT_IN 1
#define TEAPOT_WAIT_OUT 2

typedef struct { void *udata; int events; } tp_wait_event;
typedef struct { int kq; } tp_wait;

static int tp_kq_set(int kq, stb_teapot_socket_t fd, int events, void *udata)
{
    struct kevent ch[2];
    int n = 0;
    if (events & TEAPOT_WAIT_IN)
        EV_SET(&ch[n++], (uintptr_t)fd, EVFILT_READ, EV_ADD, 0, 0, udata);
    if (events & TEAPOT_WAIT_OUT)
        EV_SET(&ch[n++], (uintptr_t)fd, EVFILT_WRITE, EV_ADD, 0, 0, udata);
    return n && kevent(kq, ch, n, NULL, 0, NULL) == 0 ? 0 : -1;
}

static void tp_kq_clr(int kq, stb_teapot_socket_t fd)
{
    struct kevent ch;
    EV_SET(&ch, (uintptr_t)fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    (void)kevent(kq, &ch, 1, NULL, 0, NULL);
    EV_SET(&ch, (uintptr_t)fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    (void)kevent(kq, &ch, 1, NULL, 0, NULL);
}

int tp_wait_create(tp_wait *w) { w->kq = kqueue(); return w->kq < 0 ? -1 : 0; }
int tp_wait_add(tp_wait *w, stb_teapot_socket_t fd, int events, void *udata)
{ return tp_kq_set(w->kq, fd, events, udata); }
int tp_wait_mod(tp_wait *w, stb_teapot_socket_t fd, int events, void *udata)
{ tp_kq_clr(w->kq, fd); return tp_kq_set(w->kq, fd, events, udata); }
int tp_wait_del(tp_wait *w, stb_teapot_socket_t fd) { tp_kq_clr(w->kq, fd); return 0; }

int tp_wait_wait(tp_wait *w, int timeout_ms, tp_wait_event *out, int max_out)
{
    struct kevent buf[64];
    struct timespec ts, *tsp = NULL;
    int lim = max_out < 64 ? max_out : 64, n, i, k = 0;
    if (timeout_ms >= 0) {
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;
        tsp = &ts;
    }
    do n = kevent(w->kq, NULL, 0, buf, lim, tsp); while (n < 0 && errno == EINTR);
    if (n <= 0) return n;
    for (i = 0; i < n; ++i) {
        int events = 0;
        if (buf[i].filter == EVFILT_READ) events |= TEAPOT_WAIT_IN;
        if (buf[i].filter == EVFILT_WRITE) events |= TEAPOT_WAIT_OUT;
        if (!events) continue;
        out[k].udata = buf[i].udata;
        out[k++].events = events;
    }
    return k;
}

void tp_wait_destroy(tp_wait *w)
{
    if (w->kq >= 0) close(w->kq);
    memset(w, 0, sizeof(*w));
    w->kq = -1;
}
#define TP_WAIT_READY 1
#endif
#endif

#if TEAPOT_WAIT == TEAPOT_WAIT_WSAPOLL
#ifndef _WIN32
#error "TEAPOT_WAIT_WSAPOLL requires Windows"
#else
#define TEAPOT_WAIT_IN 1
#define TEAPOT_WAIT_OUT 2
#define TP_WE(e) ((SHORT)(((e) & TEAPOT_WAIT_IN ? POLLIN : 0) | ((e) & TEAPOT_WAIT_OUT ? POLLOUT : 0)))

typedef struct { void *udata; int events; } tp_wait_event;
typedef struct { WSAPOLLFD *pfds; void **udata; int count; int cap; } tp_wait;

static int tp_wait_find(tp_wait *w, stb_teapot_socket_t fd)
{
    int i;
    for (i = 0; i < w->count; ++i)
        if (w->pfds[i].fd == fd) return i;
    return -1;
}

int tp_wait_create(tp_wait *w) { memset(w, 0, sizeof(*w)); return 0; }

int tp_wait_add(tp_wait *w, stb_teapot_socket_t fd, int events, void *udata)
{
    if (w->count == w->cap) {
        int cap = w->cap ? w->cap * 2 : 16;
        WSAPOLLFD *pfds = TP_REALLOC(w->pfds, (size_t)cap * sizeof(*pfds));
        void **ud;
        if (!pfds) return -1;
        w->pfds = pfds;
        ud = TP_REALLOC(w->udata, (size_t)cap * sizeof(*ud));
        if (!ud) return -1;
        w->udata = ud;
        w->cap = cap;
    }
    w->pfds[w->count].fd = fd;
    w->pfds[w->count].events = TP_WE(events);
    w->pfds[w->count].revents = 0;
    w->udata[w->count++] = udata;
    return 0;
}

int tp_wait_mod(tp_wait *w, stb_teapot_socket_t fd, int events, void *udata)
{
    int i = tp_wait_find(w, fd);
    if (i < 0) return -1;
    w->pfds[i].events = TP_WE(events);
    w->udata[i] = udata;
    return 0;
}

int tp_wait_del(tp_wait *w, stb_teapot_socket_t fd)
{
    int i = tp_wait_find(w, fd);
    if (i < 0) return -1;
    w->count--;
    if (i != w->count) { w->pfds[i] = w->pfds[w->count]; w->udata[i] = w->udata[w->count]; }
    return 0;
}

int tp_wait_wait(tp_wait *w, int timeout_ms, tp_wait_event *out, int max_out)
{
    int n, i, k = 0;
    if (w->count == 0) {
        if (timeout_ms < 0) Sleep(INFINITE);
        else Sleep((DWORD)timeout_ms);
        return 0;
    }
    n = WSAPoll(w->pfds, (ULONG)w->count, timeout_ms);
    if (n <= 0) return n;
    for (i = 0; i < w->count && k < max_out; ++i) {
        SHORT re = w->pfds[i].revents;
        int events = 0;
        if (re & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) events |= TEAPOT_WAIT_IN;
        if (re & POLLOUT) events |= TEAPOT_WAIT_OUT;
        if (!events) continue;
        out[k].udata = w->udata[i];
        out[k++].events = events;
    }
    return k;
}

void tp_wait_destroy(tp_wait *w)
{
    TP_FREE(w->pfds);
    TP_FREE(w->udata);
    memset(w, 0, sizeof(*w));
}
#define TP_WAIT_READY 1
#endif
#endif

#if TEAPOT_WAIT == TEAPOT_WAIT_WFMO
#ifndef _WIN32
#error "TEAPOT_WAIT_WFMO requires Windows"
#else
#define TEAPOT_WAIT_IN 1
#define TEAPOT_WAIT_OUT 2
#define TP_WFMO_MAX 64
/* FD_CLOSE always when registered; FD_WRITE is edge-triggered (see select). */
#define TP_WFMO_MASK(e) \
    (((e) & TEAPOT_WAIT_IN ? (FD_ACCEPT | FD_READ) : 0L) | \
     ((e) & TEAPOT_WAIT_OUT ? FD_WRITE : 0L) | \
     ((e) ? FD_CLOSE : 0L))

typedef struct { void *udata; int events; } tp_wait_event;
typedef struct {
    WSAEVENT ev[TP_WFMO_MAX];
    stb_teapot_socket_t fd[TP_WFMO_MAX];
    void *udata[TP_WFMO_MAX];
    int interest[TP_WFMO_MAX];
    int count;
} tp_wait;

static int tp_wait_find(tp_wait *w, stb_teapot_socket_t fd)
{
    int i;
    for (i = 0; i < w->count; ++i)
        if (w->fd[i] == fd) return i;
    return -1;
}

/* FD_WRITE is edge-triggered: arm the event so an already-writable socket wakes. */
static int tp_wfmo_select(stb_teapot_socket_t fd, WSAEVENT ev, int events)
{
    if (WSAEventSelect(fd, ev, TP_WFMO_MASK(events)) != 0) return -1;
    if (events & TEAPOT_WAIT_OUT) (void)WSASetEvent(ev);
    return 0;
}

int tp_wait_create(tp_wait *w) { memset(w, 0, sizeof(*w)); return 0; }

int tp_wait_add(tp_wait *w, stb_teapot_socket_t fd, int events, void *udata)
{
    WSAEVENT ev;
    if (w->count >= TP_WFMO_MAX) return -1;
    ev = WSACreateEvent();
    if (ev == WSA_INVALID_EVENT) return -1;
    if (tp_wfmo_select(fd, ev, events) != 0) {
        WSACloseEvent(ev);
        return -1;
    }
    w->ev[w->count] = ev;
    w->fd[w->count] = fd;
    w->udata[w->count] = udata;
    w->interest[w->count++] = events;
    return 0;
}

int tp_wait_mod(tp_wait *w, stb_teapot_socket_t fd, int events, void *udata)
{
    int i = tp_wait_find(w, fd);
    if (i < 0) return -1;
    if (tp_wfmo_select(fd, w->ev[i], events) != 0) return -1;
    w->udata[i] = udata;
    w->interest[i] = events;
    return 0;
}

int tp_wait_del(tp_wait *w, stb_teapot_socket_t fd)
{
    int i = tp_wait_find(w, fd);
    if (i < 0) return -1;
    (void)WSAEventSelect(fd, NULL, 0);
    WSACloseEvent(w->ev[i]);
    w->count--;
    if (i != w->count) {
        w->ev[i] = w->ev[w->count];
        w->fd[i] = w->fd[w->count];
        w->udata[i] = w->udata[w->count];
        w->interest[i] = w->interest[w->count];
    }
    return 0;
}

int tp_wait_wait(tp_wait *w, int timeout_ms, tp_wait_event *out, int max_out)
{
    DWORD to = timeout_ms < 0 ? WSA_INFINITE : (DWORD)timeout_ms, n;
    WSANETWORKEVENTS ne;
    int idx, events = 0;
    if (w->count == 0) {
        if (timeout_ms < 0) Sleep(INFINITE);
        else Sleep(to);
        return 0;
    }
    n = WSAWaitForMultipleEvents((DWORD)w->count, w->ev, FALSE, to, FALSE);
    if (n == WSA_WAIT_TIMEOUT) return 0;
    if (n == WSA_WAIT_FAILED) return -1;
    idx = (int)(n - WSA_WAIT_EVENT_0);
    if (idx < 0 || idx >= w->count) return -1;
    /* Enum resets the event; on failure ResetEvent so we do not busy-loop. */
    if (WSAEnumNetworkEvents(w->fd[idx], w->ev[idx], &ne) != 0) {
        (void)ResetEvent(w->ev[idx]);
        return 0;
    }
    if (ne.lNetworkEvents & (FD_ACCEPT | FD_READ | FD_CLOSE)) events |= TEAPOT_WAIT_IN;
    if (ne.lNetworkEvents & FD_WRITE) events |= TEAPOT_WAIT_OUT;
    /* Synthetic WSASetEvent for already-writable OUT interest. */
    if (!events && (w->interest[idx] & TEAPOT_WAIT_OUT)) events = TEAPOT_WAIT_OUT;
    if (!events || max_out < 1) return 0;
    out[0].udata = w->udata[idx];
    out[0].events = events;
    return 1;
}

void tp_wait_destroy(tp_wait *w)
{
    int i;
    for (i = 0; i < w->count; ++i) {
        (void)WSAEventSelect(w->fd[i], NULL, 0);
        WSACloseEvent(w->ev[i]);
    }
    memset(w, 0, sizeof(*w));
}
#define TP_WAIT_READY 1
#endif
#endif

#ifdef TP_WAIT_READY

typedef struct
{
    teapot_server *server;
    tp_wait *w;
    stb_teapot_socket_t listen_sock;
    teapot_conn *slab;
    int max;
    int listen_armed;
} tp_run_ctx;

static void tp_run_disarm(tp_run_ctx *ctx)
{
    if (!ctx->listen_armed)
        return;
    tp_wait_del(ctx->w, ctx->listen_sock);
    ctx->listen_armed = 0;
}

static void tp_run_drop(tp_run_ctx *ctx, teapot_conn *c)
{
    tp_wait_del(ctx->w, c->fd);
    teapot_conn_free(c);
    teapot_close(c->fd);
    c->slot_used = 0;
    if (!ctx->listen_armed &&
        tp_wait_add(ctx->w, ctx->listen_sock, TEAPOT_WAIT_IN, NULL) == 0)
        ctx->listen_armed = 1;
}

static int tp_run_walk_reads(tp_run_ctx *ctx, int drop)
{
    int ms = 250, i;
    uint64_t now = tp_now_ms();
    for (i = 0; i < ctx->max; ++i)
    {
        teapot_conn *c = &ctx->slab[i];
        uint64_t rem;
        if (!c->slot_used || !c->deadline_ms)
            continue;
        if (c->phase != TEAPOT_CONN_READ_HEAD && c->phase != TEAPOT_CONN_READ_BODY)
            continue;
        if (now >= c->deadline_ms)
        {
            if (drop)
                tp_run_drop(ctx, c);
            else
                return 0;
            continue;
        }
        rem = c->deadline_ms - now;
        if (rem < (uint64_t)ms)
            ms = (int)rem;
        if (ms < 1)
            ms = 1;
    }
    return ms;
}

static void tp_run_accept(tp_run_ctx *ctx)
{
    for (;;)
    {
        int slot;
        stb_teapot_socket_t client;
        teapot_conn *c;
        if (ctx->server->stop)
            return;
        for (slot = 0; slot < ctx->max; ++slot)
            if (!ctx->slab[slot].slot_used)
                break;
        if (slot == ctx->max)
        {
            tp_run_disarm(ctx);
            return;
        }
        client = teapot_listener_accept(ctx->listen_sock);
        if (!teapot_socket_ok(client))
        {
            if (!teapot_would_block())
                tp_run_disarm(ctx);
            return;
        }
        c = &ctx->slab[slot];
        if (teapot_set_nonblock(client) != 0)
        {
            teapot_close(client);
            tp_run_disarm(ctx);
            return;
        }
        teapot_conn_init(c, ctx->server, client);
        c->slot_used = 1;
        if (tp_wait_add(ctx->w, client, TEAPOT_WAIT_IN, c) != 0)
        {
            teapot_conn_free(c);
            c->slot_used = 0;
            teapot_close(client);
            tp_run_disarm(ctx);
            return;
        }
    }
}

int teapot_run(teapot_server *server)
{
    stb_teapot_socket_t listen_sock;
    tp_wait w;
    teapot_conn *slab;
    tp_run_ctx ctx;
    int max, i;
    tp_wait_event ev[64];

    if (!server)
        return 1;
    if (teapot_listener_open(server, &listen_sock) < 0)
        return 1;
    if (teapot_set_nonblock(listen_sock) != 0 || tp_wait_create(&w) != 0)
    {
        teapot_listener_close(listen_sock);
        return 1;
    }
    max = server->max_conns > 0 ? server->max_conns : TEAPOT_MAX_CONNS;
    slab = TP_REALLOC(NULL, (size_t)max * sizeof(*slab));
    if (!slab || tp_wait_add(&w, listen_sock, TEAPOT_WAIT_IN, NULL) != 0)
    {
        TP_FREE(slab);
        tp_wait_destroy(&w);
        teapot_listener_close(listen_sock);
        return 1;
    }
    memset(slab, 0, (size_t)max * sizeof(*slab));
    ctx = (tp_run_ctx){server, &w, listen_sock, slab, max, 1};
    while (!server->stop)
    {
        int n = tp_wait_wait(&w, tp_run_walk_reads(&ctx, 0), ev, 64);
        tp_run_walk_reads(&ctx, 1);
        if (server->stop)
            break;
        if (n < 0)
            continue;
        for (i = 0; i < n; ++i)
        {
            teapot_conn *c = ev[i].udata;
            teapot_io io;
            if (!c)
            {
                tp_run_accept(&ctx);
                continue;
            }
            if (!c->slot_used)
                continue;
            io = teapot_conn_step(c);
            if (io == TEAPOT_IO_NEED_READ)
                tp_wait_mod(&w, c->fd, TEAPOT_WAIT_IN, c);
            else if (io == TEAPOT_IO_NEED_WRITE)
                tp_wait_mod(&w, c->fd, TEAPOT_WAIT_OUT, c);
            else
                tp_run_drop(&ctx, c);
        }
    }
    for (i = 0; i < max; ++i)
    {
        if (!slab[i].slot_used)
            continue;
        tp_wait_del(&w, slab[i].fd);
        teapot_conn_free(&slab[i]);
        teapot_close(slab[i].fd);
    }
    TP_FREE(slab);
    tp_wait_destroy(&w);
    teapot_listener_close(listen_sock);
    return 0;
}

#endif
#endif // STB_TEAPOT_IMPLEMENTATION

#ifdef __cplusplus
}
#endif
#endif // STB_TEAPOT_H
