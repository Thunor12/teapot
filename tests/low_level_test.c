#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

#include <stdio.h>

#ifdef _WIN32
typedef SOCKET stb_teapot_socket_t;
#endif

void stb_teapot_init()
{
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
}

stb_teapot_socket_t stb_teapot_socket(int af, int type, int protocol)
{
    return socket(af, type, protocol);
}

stb_teapot_socket_t stb_teapot_accept(stb_teapot_socket_t s, struct sockaddr *addr, int *addrlen)
{
    return accept(s, addr, addrlen);
}

int stb_teapot_read(stb_teapot_socket_t s, char *buf, int len, int flags)
{
#ifdef _WIN32
    return recv(s, buf, len, flags);
#else
    (void)flags; // ignore flags on POSIX
    return read(s, buf, len);
#endif
}

int stb_teapot_write(stb_teapot_socket_t s, const char *buf, int len, int flags)
{
#ifdef _WIN32
    return send(s, buf, len, flags);
#else
    return write(s, buf, len);
#endif
}

int stb_teapot_close_socket(stb_teapot_socket_t s)
{
#ifdef _WIN32
    return closesocket(s);
#else
    return close(s);
#endif
}

int main()
{
    stb_teapot_init();

    int port = 8080;

    stb_teapot_socket_t server_sock = stb_teapot_socket(AF_INET, SOCK_STREAM, 0);

    if (server_sock < 0)
    {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        perror("bind");
        return 1;
    }
    if (listen(server_sock, 5) != 0)
    {
        perror("listen");
        return 1;
    }

    printf("Server listening on port %d...\n", port);

    while (1)
    {
        stb_teapot_socket_t client_sock = stb_teapot_accept(server_sock, NULL, NULL);

        if (client_sock < 0)
        {
            perror("accept");
            continue;
        }

        char buffer[1024] = {0};
        int received = stb_teapot_read(client_sock, buffer, sizeof(buffer) - 1, 0);

        if (received > 0)
        {
            buffer[received] = '\0';
            printf("Received:\n%s\n", buffer);

            const char *response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 12\r\n"
                "\r\n"
                "Hello World\n";

            stb_teapot_write(client_sock, response, strlen(response), 0);

            stb_teapot_close_socket(client_sock);
        }
    }

    stb_teapot_close_socket(server_sock);

    return 0;
}
