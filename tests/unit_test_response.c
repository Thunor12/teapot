#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <unistd.h>
#endif

static int failures = 0;

static void ok(const char *name, int cond)
{
    if (cond)
    {
        printf("[PASS] %s\n", name);
    }
    else
    {
        printf("[FAIL] %s\n", name);
        ++failures;
    }
}

#ifndef _WIN32
static int read_all(int fd, char *buf, size_t cap)
{
    size_t total = 0;
    while (total + 1 < cap)
    {
        ssize_t n = read(fd, buf + total, cap - total - 1);
        if (n < 0)
        {
            return -1;
        }
        if (n == 0)
        {
            break;
        }
        total += (size_t)n;
    }
    buf[total] = '\0';
    return (int)total;
}
#endif

int main(void)
{
#ifdef _WIN32
    printf("Skipping response socketpair tests on Windows.\n");
    return 0;
#else
    int sv[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
    {
        perror("socketpair");
        return 1;
    }

    char content_type[320];
    for (size_t i = 0; i + 1 < sizeof(content_type); ++i)
    {
        content_type[i] = 'a';
    }
    content_type[sizeof(content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "OK", 2);

    int send_result = teapot_send_response((stb_teapot_socket_t)sv[0], &resp);
    shutdown(sv[0], SHUT_WR);

    char response[1024];
    int received = read_all(sv[1], response, sizeof(response));

    ok("send response with long content type", send_result == 0);
    ok("read response bytes", received > 0);
    ok("response includes status line", strstr(response, "HTTP/1.1 200 OK\r\n") == response);
    ok("response includes full long content type", strstr(response, content_type) != NULL);
    ok("response includes body after headers", strstr(response, "\r\n\r\nOK") != NULL);

    teapot_response_free(&resp);
    close(sv[0]);
    close(sv[1]);
    return failures == 0 ? 0 : 1;
#endif
}
