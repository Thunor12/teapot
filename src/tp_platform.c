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
        return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)(ts.tv_nsec / 1000000ull);
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
