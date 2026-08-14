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
        size_t content_length; /* from Content-Length, 0 if absent */
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
        int prefix; /* 0 = exact match (zero-init keeps current call sites working) */
    } teapot_route;

    typedef struct
    {
        int port;
        const teapot_route *routes;
        size_t route_count;
    } teapot_server;

    // =====================================================
    // 🧠 API
    // =====================================================
    int teapot_listen(teapot_server *server);
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

#ifdef STB_TEAPOT_IMPLEMENTATION

#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
    int teapot_socket_ok(stb_teapot_socket_t s)
    {
        return s != INVALID_SOCKET;
    }
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
    int teapot_socket_ok(stb_teapot_socket_t s)
    {
        return s >= 0;
    }
#endif

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

    void tp_extract_header_keyval(tp_headers *headers_parsed, const char *raw_header, size_t header_size)
    {
        if (headers_parsed == NULL || raw_header == NULL || header_size == 0)
        {
            return;
        }

        /* optional global clamp if defined */
#ifdef TP_MAX_HEADER_TOTAL
        if (header_size > (size_t)TP_MAX_HEADER_TOTAL)
            header_size = (size_t)TP_MAX_HEADER_TOTAL;
#endif

        /* scan line by line and use helper to parse each non-empty line */
        size_t i = 0;
        while (i < header_size)
        {
            size_t line_start = i;
            size_t line_end = line_start;

            // find end of line
            while ((line_end < header_size) && (raw_header[line_end] != '\r') && (raw_header[line_end] != '\n'))
            {
                ++line_end;
            }

            // extract line
            size_t linelen = (line_end > line_start) ? (size_t)(line_end - line_start) : 0;
            if (linelen > 0)
            {
                tp_parse_and_append_header_line(headers_parsed, raw_header + line_start, linelen);
            }

            /* advance past CR/LF */
            i = line_end;
            if (i < header_size && raw_header[i] == '\r')
            {
                ++i;
            }

            if (i < header_size && raw_header[i] == '\n')
            {
                ++i;
            }
        }
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

    static const char *tp_find_bytes(const char *buf, size_t n, const char *pat, size_t patn)
    {
        if (buf == NULL || pat == NULL || patn == 0 || n < patn)
            return NULL;
        const char *end = buf + n;
        while (buf + patn <= end)
        {
            const char *p = (const char *)memchr(buf, pat[0], (size_t)(end - buf));
            if (!p || p + patn > end)
                return NULL;
            if (memcmp(p, pat, patn) == 0)
                return p;
            buf = p + 1;
        }
        return NULL;
    }

    static int teapot_content_length_from_headers(const tp_headers *h, size_t *out_length)
    {
        const tp_string_builder *val = tp_headers_get(h, "Content-Length");
        *out_length = 0;
        if (val == NULL || val->items == NULL || val->items[0] == '\0')
            return 0;
        char *end = NULL;
        unsigned long n = strtoul(val->items, &end, 10);
        if (end == val->items)
            return -1;
        while (*end != '\0' && isspace((unsigned char)*end))
            ++end;
        if (*end != '\0' || n > (unsigned long)TEAPOT_MAX_BODY_SIZE)
            return -1;
        *out_length = (size_t)n;
        return 0;
    }

    static void free_request(teapot_request *req)
    {
        tp_sb_free(req->path);
        tp_sb_free(req->body);
        tp_headers_free(&req->headers);
    }

    static int parse_request(char *buffer, size_t size, teapot_request *req)
    {
        if (buffer == NULL || size == 0 || req == NULL)
            return -1;

        req->path = (tp_string_builder){0};
        req->body = (tp_string_builder){0};
        req->headers = (tp_headers){0};
        req->body_length = 0;
        req->content_length = 0;

        const char *header_end = tp_find_bytes(buffer, size, "\r\n\r\n", 4);
        const char *request_line_end = tp_find_bytes(buffer, size, "\r\n", 2);
        if (header_end == NULL || request_line_end == NULL || request_line_end > header_end)
            return -1;

        size_t line_n = (size_t)(request_line_end - buffer);
        const char *sp1 = (const char *)memchr(buffer, ' ', line_n);
        if (!sp1)
            return -1;
        teapot_method method = parse_method(buffer, (size_t)(sp1 - buffer));
        if (method == TEAPOT_UNKNOWN)
            return -1;

        const char *path0 = sp1 + 1;
        size_t rest = (size_t)(request_line_end - path0);
        const char *sp2 = (const char *)memchr(path0, ' ', rest);
        if (!sp2)
            return -1;
        size_t path_n = (size_t)(sp2 - path0);
        if (path_n == 0)
            return -1;

        const char *headers_start = request_line_end + 2;
        size_t header_size = header_end > headers_start ? (size_t)(header_end - headers_start) : 0;
        tp_extract_header_keyval(&req->headers, headers_start, header_size);

        size_t content_length = 0;
        if (teapot_content_length_from_headers(&req->headers, &content_length) != 0)
            return -1;

        const char *body_start = header_end + 4;
        size_t body_available = 0;
        if (size > (size_t)(body_start - buffer))
            body_available = size - (size_t)(body_start - buffer);
        size_t to_append = content_length < body_available ? content_length : body_available;

        req->method = method;
        req->content_length = content_length;
        tp_sb_append_buf(&req->path, path0, path_n);
        tp_sb_append_null(&req->path);
        tp_sb_append_buf(&req->body, body_start, to_append);
        tp_sb_append_null(&req->body);
        req->body_length = to_append;
        return 0;
    }

    static int teapot_complete_request_body(stb_teapot_socket_t client, teapot_request *req)
    {
        if (req->body_length >= req->content_length)
            return 0;
        req->body.count = req->body_length;
        while (req->body_length < req->content_length)
        {
            char read_buf[4096];
            size_t to_read = req->content_length - req->body_length;
            if (to_read > sizeof(read_buf))
                to_read = sizeof(read_buf);
            int n = teapot_read(client, read_buf, (int)to_read);
            if (n <= 0)
                return -1;
            tp_sb_append_buf(&req->body, read_buf, (size_t)n);
            req->body_length += (size_t)n;
        }
        tp_sb_append_null(&req->body);
        return 0;
    }

    static void teapot_send_status_body(stb_teapot_socket_t client, int status, const char *msg)
    {
        teapot_response resp;
        teapot_response_init(&resp, status);
        tp_sb_appendf(&resp.body, "%s", msg);
        (void)teapot_send_response(client, &resp);
        teapot_response_free(&resp);
    }

    // -----------------------------------------------------
    // 🧭 Find Matching Route
    // -----------------------------------------------------
    static teapot_handler teapot_find_handler(teapot_server *server, teapot_request *req)
    {
        for (size_t i = 0; i < server->route_count; i++)
        {
            const teapot_route *r = &server->routes[i];
            if (r->method != req->method)
                continue;
            size_t path_len = strlen(r->path);
            /* Prefix match: path ending with '*' matches any path that starts with the prefix (minus '*') */
            if (path_len > 0 && r->path[path_len - 1] == '*')
            {
                size_t prefix_len = path_len - 1;
                if (strncmp(r->path, req->path.items, prefix_len) == 0 &&
                    (req->path.items[prefix_len] == '\0' || req->path.items[prefix_len] == '/' || req->path.items[prefix_len] == '?'))
                {
                    return r->handler;
                }
            }
            else if (strcmp(r->path, req->path.items) == 0)
            {
                return r->handler;
            }
        }
        return NULL;
    }

    // -----------------------------------------------------
    // 🫖 Listen Loop
    // -----------------------------------------------------

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
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            perror("bind");
            teapot_close(s);
            return -1;
        }

        if (listen(s, 8) < 0)
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

        const char *ct = (resp->content_type != NULL) ? resp->content_type : "text/plain";
        if (strpbrk(ct, "\r\n") != NULL)
            return -1;

        tp_string_builder header = {0};
        tp_sb_appendf(&header,
                      "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: " TP_SIZE_T_FMT "\r\n\r\n",
                      resp->status, teapot_status_to_str(resp->status), ct, tp_da_len(resp->body));

        int rc = teapot_write_all(client, header.items, header.count);
        if (rc == 0 && resp->body.count > 0)
            rc = teapot_write_all(client, resp->body.items, resp->body.count);

        tp_sb_free(header);
        return rc;
    }

    int teapot_serve_client(teapot_server *server, stb_teapot_socket_t client)
    {
        if (!server || !teapot_socket_ok(client))
            return -1;

        char buffer[8192] = {0};
        int received = 0;
        if (teapot_recv_request(client, buffer, (int)sizeof(buffer), &received) < 0)
            return -1;

        teapot_request req = {0};
        if (parse_request(buffer, (size_t)received, &req) < 0)
        {
            free_request(&req);
            teapot_send_status_body(client, TEAPOT_HTTP_BAD_REQUEST, "400 Bad Request\n");
            return -1;
        }

        if (teapot_complete_request_body(client, &req) != 0)
        {
            free_request(&req);
            teapot_send_status_body(client, TEAPOT_HTTP_BAD_REQUEST, "400 Bad Request\n");
            return -1;
        }

        teapot_handler handler = teapot_find_handler(server, &req);
        teapot_response resp;
        teapot_response_init(&resp, TEAPOT_HTTP_OK);
        if (handler)
            resp = handler(&req);
        else
        {
            resp.status = TEAPOT_HTTP_NOT_FOUND;
            tp_sb_appendf(&resp.body, "404 Not Found\n");
        }

        int rc = teapot_send_response(client, &resp);
        teapot_response_free(&resp);
        free_request(&req);
        return rc;
    }

    int teapot_handle_client_connection(teapot_server *server, stb_teapot_socket_t client)
    {
        int rc = teapot_serve_client(server, client);
        teapot_close(client);
        return rc;
    }

    int teapot_listen(teapot_server *server)
    {
        if (!server)
            return 1;

        stb_teapot_socket_t listen_sock;
        if (teapot_listener_open(server, &listen_sock) < 0)
            return 1;

        printf("stb_teapot listening on port %d\n", server->port);

        while (1)
        {
            stb_teapot_socket_t client = teapot_listener_accept(listen_sock);
            if ((int)client < 0)
                continue;
            teapot_handle_client_connection(server, client);
        }

        return 0;
    }

#endif // STB_TEAPOT_IMPLEMENTATION

#ifdef __cplusplus
}
#endif
#endif // STB_TEAPOT_H
