#include "teapot.h"

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
#include <sys/time.h>
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
