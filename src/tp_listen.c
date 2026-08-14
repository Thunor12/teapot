#include "teapot.h"

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
