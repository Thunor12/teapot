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

    static void parse_request_line(const char *buf, teapot_request *req)
    {
        const char *space1 = strchr(buf, ' ');
        if (!space1)
        {
            req->method = TEAPOT_UNKNOWN;
            req->path = "";
            return;
        }
        req->method = parse_method(buf);
        const char *space2 = strchr(space1 + 1, ' ');
        if (!space2)
        {
            req->path = "";
            return;
        }
        static char path_buf[256];
        size_t len = space2 - (space1 + 1);
        if (len >= sizeof(path_buf))
            len = sizeof(path_buf) - 1;
        memcpy(path_buf, space1 + 1, len);
        path_buf[len] = '\0';
        req->path = path_buf;
        req->body = NULL;
        req->body_length = 0;
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

        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(server->port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            perror("bind");
            return 1;
        }
        if (listen(sock, 5) < 0)
        {
            perror("listen");
            return 1;
        }

        printf("Server listening on port %d...\n", server->port);

        while (1)
        {
            stb_teapot_socket_t client = accept(sock, NULL, NULL);
            if (client < 0)
            {
                perror("accept");
                continue;
            }

            char buffer[1024] = {0};
            int received = teapot_read(client, buffer, sizeof(buffer) - 1);
            if (received > 0)
            {
                buffer[received] = '\0';
                teapot_request req;
                parse_request_line(buffer, &req);
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
