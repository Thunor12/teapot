#include "teapot.h"

#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

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
            if (!teapot_socket_ok(client))
                continue;
            teapot_handle_client_connection(server, client);
        }

        return 0;
    }
