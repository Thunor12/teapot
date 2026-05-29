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

    int socket_ok(stb_teapot_socket_t s);
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int stb_teapot_socket_t;
int socket_ok(stb_teapot_socket_t s);

#endif

// Initial capacity of a dynamic array
#ifndef TP_DA_INIT_CAP
#define TP_DA_INIT_CAP 256
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
#define tp_da_capacity(da) ((da).capacity)

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
#define tp_da_append_many(da, new_items, new_items_count)                                         \
    do                                                                                            \
    {                                                                                             \
        tp_da_reserve((da), (da)->count + (new_items_count));                                     \
        memcpy((da)->items + (da)->count, (new_items), (new_items_count) * sizeof(*(da)->items)); \
        (da)->count += (new_items_count);                                                         \
    } while (0)

#define tp_da_resize(da, new_size)     \
    do                                 \
    {                                  \
        tp_da_reserve((da), new_size); \
        (da)->count = (new_size);      \
    } while (0)

#define tp_da_last(da) (da)->items[(TP_ASSERT((da)->count > 0), (da)->count - 1)]
#define tp_da_remove_unordered(da, i)                \
    do                                               \
    {                                                \
        size_t j = (i);                              \
        TP_ASSERT(j < (da)->count);                  \
        (da)->items[j] = (da)->items[--(da)->count]; \
    } while (0)

    typedef struct
    {
        char *items;
        size_t count;
        size_t capacity;
    } tp_string_builder;

    typedef struct
    {
        char **items;
        size_t count;
        size_t capacity;
    } tp_string_array;

#define tp_sa_append_str(sa, str, str_len)                                \
    do                                                                    \
    {                                                                     \
        char *s = TP_DECLTYPE_CAST(char *) TP_REALLOC(NULL, str_len + 1); \
        memcpy(s, str, str_len);                                          \
        s[str_len] = '\0';                                                \
        tp_da_append(sa, s);                                              \
    } while (0)

#define tp_sa_free(sa)                          \
    do                                          \
    {                                           \
        for (size_t i = 0; i < (sa).count; i++) \
        {                                       \
            TP_FREE((sa).items[i]);             \
        }                                       \
        TP_FREE((sa).items);                    \
    } while (0)

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

    /* Return 1 if header 'name' exists and its value equals 'expected_value', otherwise 0 */
    int tp_headers_match(const tp_headers *h, const char *name, const char *expected_value);

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
    int teapot_recv_request(stb_teapot_socket_t client, char *buffer, int bufsize, int *out_received);
    int teapot_send_response(stb_teapot_socket_t client, const teapot_response *resp);
    int teapot_handle_client_connection(teapot_server *server, stb_teapot_socket_t client);

#ifdef STB_TEAPOT_IMPLEMENTATION

#include <stdarg.h>
/* portable case-insensitive compare helper */
#include <ctype.h>

#ifdef _WIN32
#include <winsock2.h>
    int socket_ok(stb_teapot_socket_t s)
    {
        return s != INVALID_SOCKET;
    }
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
    int socket_ok(stb_teapot_socket_t s)
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

    /* Return 1 if header 'name' exists and its value equals 'expected_value', otherwise 0 */
    int tp_headers_match(const tp_headers *h, const char *name, const char *expected_value)
    {
        if (h == NULL || name == NULL || expected_value == NULL)
            return 0;
        const tp_string_builder *val = tp_headers_get(h, name);
        if (val == NULL || val->items == NULL)
            return 0;
        return strcmp(val->items, expected_value) == 0 ? 1 : 0;
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
        if (n < 0)
        {
            return n;
        }

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

    // TODO: See if we can do without this function entirely
    void tp_chop_by_delim_into_array(tp_string_array *sa, char *src, size_t src_len, const char *delim)
    {
        if (!sa || !src || !delim)
        {
            return;
        }

        tp_string_builder sb = {0};
        tp_sb_append_buf(&sb, src, src_len);

        *sa = (tp_string_array){0};

        char *token = strtok(sb.items, delim);
        if (token)
        {
            do
            {
                tp_sa_append_str(sa, token, strlen(token));
            } while ((token = strtok(NULL, delim)));
        }

        tp_sb_free(sb);
    }

#if 0
    /* helpers for header parsing (static, internal) */
    static size_t tp_trim_trailing_ws(const char *s, size_t len)
    {
        // trim trailing whitespace
        while (len > 0 && isspace((unsigned char)s[len - 1]))
        {
            --len;
        }
        return len;
    }

#endif

    static size_t tp_trim_leading_ws(const char *s, size_t len)
    {
        size_t start = 0;
        while (start < len && isspace((unsigned char)s[start]))
        {
            ++start;
        }
        return start;
    }

    static size_t tp_trim_ws(const char *s, size_t len)
    {
        size_t start = tp_trim_leading_ws(s, len);
        size_t end = len;
        while (end > start && isspace((unsigned char)s[end - 1]))
        {
            --end;
        }
        return end - start;
    }

    // TODO: handle folded headers (lines starting with SP/HT are continuations of previous header)
    // TODO: handle multiple headers with same name (append to existing value with comma separation)
    // TODO: handle invalid headers gracefully
    // TODO: handle overly long headers gracefully
    // TODO: handle invalid characters gracefully
    // TODO: handle non-ASCII characters gracefully
    // TODO: handle different line endings (\r\n, \n, \r) gracefully
    // TODO: handle empty header names gracefully
    // TODO: handle empty header values gracefully
    // TODO: handle headers with no value (e.g., "X-Flag:") gracefully
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

        const char *name_start = line;
        size_t name_len = (size_t)(colon - name_start);
        size_t name_leading_ws = tp_trim_leading_ws(name_start, name_len);
        name_start += name_leading_ws;
        name_len -= name_leading_ws;
        while (name_len > 0 && isspace((unsigned char)name_start[name_len - 1]))
        {
            --name_len;
        }
        if (name_len == 0)
        {
            return 0;
        }

        /* value: skip ':' and leading whitespace, then trim trailing whitespace */
        const char *vstart = colon + 1;
        const char *line_end = line + linelen;
        while (vstart < line_end && isspace((unsigned char)*vstart))
        {
            ++vstart;
        }
        size_t vlen = (vstart < line_end) ? (size_t)(line_end - vstart) : 0;
        vlen = tp_trim_ws(vstart, vlen);

        /* clamp to configured maxima */
#ifndef TP_MAX_HEADER_NAME_LEN
#define TP_MAX_HEADER_NAME_LEN 256
#endif
#ifndef TP_MAX_HEADER_VALUE_LEN
#define TP_MAX_HEADER_VALUE_LEN 4096
#endif
        if (name_len > (size_t)TP_MAX_HEADER_NAME_LEN)
        {
            name_len = (size_t)TP_MAX_HEADER_NAME_LEN;
        }

        if (vlen > (size_t)TP_MAX_HEADER_VALUE_LEN)
        {
            vlen = (size_t)TP_MAX_HEADER_VALUE_LEN;
        }

        tp_header_line header_line = {0};
        if (name_len)
        {
            tp_sb_append_buf(&header_line.name, name_start, name_len);
            tp_sb_append_null(&header_line.name);
        }
        if (vlen)
        {
            tp_sb_append_buf(&header_line.value, vstart, vlen);
            tp_sb_append_null(&header_line.value);
        }
        tp_da_append(headers_parsed, header_line);
        return 1;
    }

    void tp_extract_header_keyval(tp_headers *headers_parsed, char *raw_header, size_t header_size)
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

    static int tp_parse_content_length(const char *value, size_t *out_length)
    {
        if (value == NULL || out_length == NULL)
        {
            return 0;
        }

        while (isspace((unsigned char)*value))
        {
            ++value;
        }

        if (!isdigit((unsigned char)*value))
        {
            return 0;
        }

        size_t result = 0;
        while (isdigit((unsigned char)*value))
        {
            size_t digit = (size_t)(*value - '0');
            if (result > (SIZE_MAX - digit) / 10U)
            {
                return 0;
            }
            result = result * 10U + digit;
            ++value;
        }

        while (isspace((unsigned char)*value))
        {
            ++value;
        }

        if (*value != '\0')
        {
            return 0;
        }

        *out_length = result;
        return 1;
    }

    tp_header_result tp_headers_check(const tp_headers *h, const char *name, const char *expected_value, tp_header_line *o_header_line)
    {
        if (expected_value == NULL)
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

            return TP_HEADER_FOUND;
        }

        const tp_header_line *hl = tp_headers_find(h, name);
        if (!hl)
        {
            return TP_HEADER_NOT_FOUND;
        }

        if (o_header_line)
        {
            *o_header_line = *hl;
        }

        const char *val = hl->value.items ? hl->value.items : "";
        return (strcmp(val, expected_value) == 0) ? TP_HEADER_MATCH : TP_HEADER_FOUND;
    }

// =====================================================
// 🛠 Implementation
// =====================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

    static void teapot_init(void)
    {
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    }

    static void teapot_close(stb_teapot_socket_t s)
    {
#ifdef _WIN32
        closesocket(s);
#else
        close(s);
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
        {
            return 0;
        }

#ifdef _WIN32
        return send(s, buf, len, 0);
#else
#ifdef MSG_NOSIGNAL
        return (int)send(s, buf, (size_t)len, MSG_NOSIGNAL);
#else
        return (int)send(s, buf, (size_t)len, 0);
#endif
#endif
    }

    static int teapot_write_all(stb_teapot_socket_t s, const char *buf, size_t len)
    {
        size_t written = 0;
        while (written < len)
        {
            size_t remaining = len - written;
            int chunk = remaining > (size_t)INT32_MAX ? INT32_MAX : (int)remaining;
            int n = teapot_write(s, buf + written, chunk);
            if (n <= 0)
            {
                return -1;
            }
            written += (size_t)n;
        }

        return 0;
    }

    // -----------------------------------------------------
    // 🧩 Request Parsing (minimal, single-line HTTP/1.0)
    // -----------------------------------------------------
    static teapot_method parse_method(const char *s)
    {
        if (strncmp(s, "GET", 3) == 0)
            return TEAPOT_GET;
        if (strncmp(s, "POST", 4) == 0)
            return TEAPOT_POST;
        if (strncmp(s, "PUT", 3) == 0)
            return TEAPOT_PUT;
        if (strncmp(s, "DELETE", 6) == 0)
            return TEAPOT_DELETE;
        return TEAPOT_UNKNOWN;
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
        {
            return -1;
        }

        /* ensure headers/builders start empty */
        req->path = (tp_string_builder){0};
        req->body = (tp_string_builder){0};
        req->headers = (tp_headers){0};

        char method_buf[8] = {0};
        char path_buf[512] = {0};

        sscanf(buffer, "%7s %511s", method_buf, path_buf);
        teapot_method method = parse_method(method_buf);
        if (method == TEAPOT_UNKNOWN)
        {
            return -1;
        }

        const char *body_start = strstr(buffer, "\r\n\r\n");

        size_t content_length = 0;
        const char *body = "";

        if (body_start)
        {
            body_start += 4;
            body = body_start;
        }

        /* extract headers from start..(body_start) */
        size_t header_size = size;
        if (body_start)
            header_size = (size_t)(body_start - buffer);
        tp_extract_header_keyval(&req->headers, buffer, header_size);
        const tp_string_builder *cl_val = tp_headers_get(&req->headers, "Content-Length");
        if (cl_val && cl_val->items)
        {
            (void)tp_parse_content_length(cl_val->items, &content_length);
        }

        req->method = method;
        tp_sb_append_buf(&req->path, path_buf, strlen(path_buf));
        tp_sb_append_null(&req->path);

        /* Only append body bytes that are actually in the buffer (body may arrive in a later packet) */
        size_t body_available = 0;
        if (body_start && size > (size_t)(body - buffer))
            body_available = size - (size_t)(body - buffer);
        size_t to_append = content_length;
        if (to_append > body_available)
            to_append = body_available;
        tp_sb_append_buf(&req->body, body, to_append);
        tp_sb_append_null(&req->body);

        req->body_length = to_append; /* may be less than Content-Length if body arrives in next packet */

        return 0;
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
        if (!socket_ok(s))
        {
            perror("socket");
            return -1;
        }

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
        if (!socket_ok((stb_teapot_socket_t)listen_sock))
        {
            return (stb_teapot_socket_t)-1;
        }

        stb_teapot_socket_t client = accept((stb_teapot_socket_t)listen_sock, NULL, NULL);
        if (!socket_ok(client))
        {
            return (stb_teapot_socket_t)-1;
        }

        return (stb_teapot_socket_t)client;
    }

    int teapot_recv_request(stb_teapot_socket_t client, char *buffer, int bufsize, int *out_received)
    {
        if (!socket_ok((stb_teapot_socket_t)client) || !buffer || bufsize <= 0)
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
        if (!socket_ok((stb_teapot_socket_t)client) || !resp)
            return -1;

        const char *ct = (resp->content_type != NULL) ? resp->content_type : "text/plain";
        tp_string_builder header = {0};
        if (tp_sb_appendf(
                &header,
                "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: " TP_SIZE_T_FMT "\r\n\r\n",
                resp->status, teapot_status_to_str(resp->status), ct, tp_da_len(resp->body)) < 0)
        {
            return -1;
        }

        if (teapot_write_all((stb_teapot_socket_t)client, header.items, header.count) < 0)
        {
            tp_sb_free(header);
            return -1;
        }
        tp_sb_free(header);

        if (resp->body.count > 0)
        {
            if (teapot_write_all((stb_teapot_socket_t)client, resp->body.items, resp->body.count) < 0)
            {
                return -1;
            }
        }
        return 0;
    }

    int teapot_handle_client_connection(teapot_server *server, stb_teapot_socket_t client)
    {
        if (!server || !socket_ok((stb_teapot_socket_t)client))
        {
            teapot_close((stb_teapot_socket_t)client);
            return -1;
        }

        char buffer[8192] = {0};
        int received = 0;
        if (teapot_recv_request(client, buffer, (int)sizeof(buffer), &received) < 0)
        {
            teapot_close((stb_teapot_socket_t)client);
            return -1;
        }

        teapot_request req = {0};
        if (parse_request(buffer, (size_t)received, &req) < 0)
        {
            free_request(&req);
            teapot_close((stb_teapot_socket_t)client);
            return -1;
        }

        /* If Content-Length says we should have more body, read remaining bytes (body may be in next packet) */
        {
            const tp_string_builder *cl_val = tp_headers_get(&req.headers, "Content-Length");
            size_t expected_body = 0;
            if (cl_val && cl_val->items)
            {
                if (tp_parse_content_length(cl_val->items, &expected_body) &&
                    expected_body > (size_t)(4 * 1024 * 1024)) /* cap 4MB */
                {
                    expected_body = 0;
                }
            }
            if (expected_body > req.body_length)
            {
                req.body.count = req.body_length; /* drop trailing null so we can append */
                while (req.body_length < expected_body)
                {
                    char read_buf[4096];
                    size_t to_read = expected_body - req.body_length;
                    if (to_read > sizeof(read_buf))
                        to_read = sizeof(read_buf);
                    int n = teapot_read((stb_teapot_socket_t)client, read_buf, (int)to_read);
                    if (n <= 0)
                        break;
                    tp_sb_append_buf(&req.body, read_buf, (size_t)n);
                    req.body_length += (size_t)n;
                }
                tp_sb_append_null(&req.body);
            }
        }

        teapot_handler handler = teapot_find_handler(server, &req);
        teapot_response resp;
        teapot_response_init(&resp, TEAPOT_HTTP_OK);

        if (handler)
        {
            resp = handler(&req);
        }
        else
        {
            resp.status = TEAPOT_HTTP_NOT_FOUND;
            tp_sb_appendf(&resp.body, "404 Not Found\n");
        }

        /* Do not append null: body is sent as-is; trailing null would break JSON parsing in clients. */

        teapot_send_response(client, &resp);

        teapot_response_free(&resp);
        free_request(&req);
        teapot_close((stb_teapot_socket_t)client);
        return 0;
    }

    // Keep a convenience blocking single-threaded listen that uses the new API
    int teapot_listen(teapot_server *server)
    {
        if (!server)
        {
            return 1;
        }

        stb_teapot_socket_t listen_sock;
        if (teapot_listener_open(server, &listen_sock) < 0)
        {
            return 1;
        }

        printf("🫖 stb_teapot listening on port %d\n", server->port);

        while (1)
        {
            stb_teapot_socket_t client = teapot_listener_accept(listen_sock);
            if ((int)client < 0)
            {
                continue;
            }

            // simple single-threaded handling (users can accept and dispatch themselves)
            teapot_handle_client_connection(server, client);
        }

        /* unreachable in current API, but keep symmetry */
        // teapot_close((stb_teapot_socket_t)listen_sock);
        return 0;
    }

#endif // STB_TEAPOT_IMPLEMENTATION

#ifdef __cplusplus
}
#endif
#endif // STB_TEAPOT_H
