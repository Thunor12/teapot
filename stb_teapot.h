#ifndef STB_TEAPOT_H
#define STB_TEAPOT_H

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
        TEAPOT_UNKNOWN
    } teapot_method;

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

    typedef struct
    {
        teapot_method method;
        tp_string_builder path;
        tp_string_builder body;
        tp_string_builder content_type;
        size_t body_length;
    } teapot_request;

    typedef struct
    {
        int status;
        tp_string_builder body;
    } teapot_response;

    static inline void teapot_response_init(teapot_response *res, int status)
    {
        res->status = status;
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

#ifdef STB_TEAPOT_IMPLEMENTATION

#include <stdarg.h>

#if 0
    static const char *teapot_status_to_str(int status)
    {
        switch (status)
        {
        case 200:
            return "OK";
        case 400:
            return "Bad Request";
        case 404:
            return "Not Found";
        case 415:
            return "Unsupported Media Type";
        default:
            return "Unknown";
        }
    }
#endif

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

    void tp_chop_by_delim_into_array(tp_string_array *sa, const char *src, const char *delim)
    {
        if (!sa || !src || !delim)
        {
            return;
        }

        tp_string_builder sb = {0};
        tp_sb_append_cstr(&sb, src);

        *sa = (tp_string_array){0};

        char *token = strtok(sb.items, delim);
        do
        {
            tp_sa_append_str(sa, token, strlen(token));
        } while ((token = strtok(NULL, delim)));

        tp_sb_free(sb);
    }

// =====================================================
// 🛠 Implementation
// =====================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef _WIN32
#include <winsock2.h>
    typedef SOCKET stb_teapot_socket_t;

    static int socket_ok(stb_teapot_socket_t s)
    {
        return s != INVALID_SOCKET;
    }
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
    typedef int stb_teapot_socket_t;
    static int socket_ok(stb_teapot_socket_t s)
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
        return (int)write(s, buf, (size_t)len);
#endif
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
        return TEAPOT_UNKNOWN;
    }

    static void free_request(teapot_request *req)
    {
        tp_sb_free(req->path);
        tp_sb_free(req->body);
        tp_sb_free(req->content_type);
    }

    static int parse_request(char *buffer, size_t size, teapot_request *req)
    {
        if (buffer == NULL || size == 0 || req == NULL)
        {
            return -1;
        }

        char method_buf[8] = {0};
        char path_buf[512] = {0};

        sscanf(buffer, "%7s %511s", method_buf, path_buf);
        int method = parse_method(method_buf);
        if (method == TEAPOT_UNKNOWN)
        {
            return -1;
        }

        const char *ct = strstr(buffer, "Content-Type:");
        const char *cl = strstr(buffer, "Content-Length:");
        const char *body_start = strstr(buffer, "\r\n\r\n");

        char content_type[128] = "";
        size_t content_length = 0;
        const char *body = "";

        if (ct)
        {
            sscanf(ct, "Content-Type: %127s", content_type);
        }

        if (cl)
        {
            sscanf(cl, "Content-Length: " TP_SIZE_T_FMT "", &content_length);
        }

        if (body_start)
        {
            body_start += 4;
            body = body_start;
        }

        if (content_length > strlen(body))
        {
            printf("Body length exceeds received data\n");
            return -1;
        }

        req->method = method;
        tp_sb_append_buf(&req->path, path_buf, strlen(path_buf));
        tp_sb_append_null(&req->path);

        tp_sb_append_buf(&req->content_type, content_type, strlen(content_type));
        tp_sb_append_null(&req->content_type);

        tp_sb_append_buf(&req->body, body, content_length);
        tp_sb_append_null(&req->body);

        req->body_length = content_length;

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
            if (r->method == req->method && strcmp(r->path, req->path.items) == 0)
            {
                return r->handler;
            }
        }
        return NULL;
    }

    // -----------------------------------------------------
    // 🫖 Listen Loop
    // -----------------------------------------------------
    int teapot_listen(teapot_server *server)
    {
        teapot_init();

        stb_teapot_socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
        if (!socket_ok(sock))
        {
            perror("socket");
            return 1;
        }

        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons((uint16_t)server->port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            perror("bind");
            teapot_close(sock);
            return 1;
        }

        if (listen(sock, 8) < 0)
        {
            perror("listen");
            teapot_close(sock);
            return 1;
        }

        printf("🫖 stb_teapot listening on port %d\n", server->port);

        char buffer[8192] = {0};
        while (1)
        {
            stb_teapot_socket_t client = accept(sock, NULL, NULL);
            if (!socket_ok(client))
            {
                continue;
            }

            int received = teapot_read(client, buffer, sizeof(buffer) - 1);
            if (received <= 0)
            {
                teapot_close(client);
                continue;
            }
            buffer[received] = '\0';

            printf("Received: %s\n", buffer);

            teapot_request req = {0};
            if (parse_request(buffer, (size_t)received, &req) < 0)
            {
                teapot_close(client);
                continue;
            }

            teapot_handler handler = teapot_find_handler(server, &req);
            teapot_response resp;
            teapot_response_init(&resp, 200);

            if (handler)
            {
                resp = handler(&req);
            }
            else
            {
                tp_sb_appendf(&resp.body, "404 Not Found\n");
            }
            tp_sb_append_null(&resp.body);

            char header[256] = {0};
            int header_len = snprintf(
                header, sizeof(header),
                "HTTP/1.1 %d OK\r\nContent-Type: text/plain\r\nContent-Length: " TP_SIZE_T_FMT "\r\n\r\n", resp.status, tp_da_len(resp.body));

            teapot_write(client, header, header_len);
            teapot_write(client, resp.body.items, (int)resp.body.count);

            teapot_response_free(&resp);
            free_request(&req);
            teapot_close(client);
        }

        teapot_close(sock);
        return 0;
    }
#endif // STB_TEAPOT_IMPLEMENTATION

#ifdef __cplusplus
}
#endif
#endif // STB_TEAPOT_H
