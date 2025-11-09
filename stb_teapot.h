#ifndef STB_TEAPOT_H
#define STB_TEAPOT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

    // --------------------------
    // HTTP Methods
    // --------------------------
    typedef enum
    {
        TEAPOT_GET,
        TEAPOT_POST,
        TEAPOT_UNKNOWN
    } teapot_method;

    // --------------------------
    // Request / Response
    // --------------------------
    typedef struct
    {
        teapot_method method;
        const char *path;
        const char *body;
        size_t body_length;
    } teapot_request;

    typedef struct
    {
        int status;       // e.g. 200, 404
        const char *body; // user-provided
    } teapot_response;

    // --------------------------
    // Route (user-owned)
    // --------------------------
    typedef teapot_response (*teapot_handler)(const teapot_request *);

    typedef struct
    {
        teapot_method method;
        const char *path; // user-owned string
        teapot_handler handler;
    } teapot_route;

    // --------------------------
    // Server (user-owned routes)
    // --------------------------
    typedef struct
    {
        int port;
        const teapot_route *routes;
        size_t route_count;
    } teapot_server;

    // --------------------------
    // API
    // --------------------------
    int teapot_listen(teapot_server *server);

#ifdef STB_TEAPOT_IMPLEMENTATION

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
    typedef SOCKET stb_teapot_socket_t;
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
    typedef int stb_teapot_socket_t;
#endif

    // --------------------------
    // Platform helpers
    // --------------------------
    static void teapot_init()
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
#ifdef _WIN32
        return recv(s, buf, len, 0);
#else
        return read(s, buf, len);
#endif
    }

    static int teapot_write(stb_teapot_socket_t s, const char *buf, int len)
    {
#ifdef _WIN32
        return send(s, buf, len, 0);
#else
        return write(s, buf, len);
#endif
    }

    // --------------------------
    // Request parsing (first line only)
    // --------------------------
    static teapot_method parse_method(const char *s)
    {
        if (strncmp(s, "GET", 3) == 0)
            return TEAPOT_GET;
        if (strncmp(s, "POST", 4) == 0)
            return TEAPOT_POST;
        return TEAPOT_UNKNOWN;
    }

    // --------------------------
    // Find matching route
    // --------------------------
    static teapot_handler teapot_find_handler(teapot_server *server, teapot_request *req)
    {
        for (size_t i = 0; i < server->route_count; i++)
        {
            const teapot_route *r = &server->routes[i];
            if (r->method == req->method && strcmp(r->path, req->path) == 0)
            {
                return r->handler;
            }
        }
        return NULL;
    }

    static int parse_request(char *buffer, size_t size, teapot_request *req)
    {
        // --- Parse request line ---
        char method_buf[8];
        char path_buf[512];

        sscanf(buffer, "%7s %511s", method_buf, path_buf);
        int method = parse_method(method_buf);

        if (method == TEAPOT_UNKNOWN)
        {
            return -1;
        }

        // --- Parse Content-Length ---
        size_t content_length = 0;
        const char *cl = strstr(buffer, "Content-Length:");
        if (cl)
        {
            sscanf(cl, "Content-Length: %zu", &content_length);
        }

        // --- Find start of body ---
        const char *body_start = strstr(buffer, "\r\n\r\n");
        const char *body = "";
        if (body_start)
        {
            body_start += 4;
            body = body_start;
        }

        *req = (teapot_request){
            .method = method,
            .path = path_buf,
            .body = body,
            .body_length = strlen(body),
        };

        return 0;
    }

    // --------------------------
    // Listen loop
    // --------------------------
    int teapot_listen(teapot_server *server)
    {
        teapot_init();

        stb_teapot_socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            perror("socket");
            return 1;
        }

        // TODO
        // int opt = 1;
        // setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(server->port);
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

        while (1)
        {
            stb_teapot_socket_t client = accept(sock, NULL, NULL);
            if (client < 0)
            {
                perror("accept");
                continue;
            }

            char buffer[1024 * 8];
            int received = teapot_read(client, buffer, sizeof(buffer) - 1);
            if (received > 0)
            {
                buffer[received] = '\0';

                teapot_request req = {0};
                if (parse_request(buffer, received, &req) < 0)
                {
                    printf("Failed to parse request\n");
                    teapot_close(client);
                    continue;
                }

                teapot_handler handler = teapot_find_handler(server, &req);
                teapot_response resp;
                if (handler)
                {
                    resp = handler(&req);
                }
                else
                {
                    resp.status = 404;
                    resp.body = "Not Found\n";
                }

                char header[256];
                int len = snprintf(
                    header,
                    sizeof(header),
                    "HTTP/1.1 %d OK\r\nContent-Type: text/plain\r\nContent-Length: %zu\r\n\r\n",
                    resp.status, strlen(resp.body));

                teapot_write(client, header, len);
                teapot_write(client, resp.body, (int)strlen(resp.body));
            }
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
