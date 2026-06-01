#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

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

static void test_long_content_type_header(void)
{
    int fds[2] = {-1, -1};
    ok("socketpair", socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    if (fds[0] < 0 || fds[1] < 0)
    {
        return;
    }

    char long_content_type[600];
    memset(long_content_type, 'a', sizeof(long_content_type) - 1);
    long_content_type[sizeof(long_content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = long_content_type;
    teapot_response_write(&resp, "OK", 2);

    ok("send response with long content type", teapot_send_response((stb_teapot_socket_t)fds[0], &resp) == 0);

    char received[1024] = {0};
    ssize_t n = read(fds[1], received, sizeof(received) - 1);
    ok("read response", n > 0);
    if (n > 0)
    {
        size_t received_len = (size_t)n;
        received[received_len] = '\0';
        ok("status line present", strstr(received, "HTTP/1.1 200 OK\r\n") != NULL);
        ok("full content type present", strstr(received, long_content_type) != NULL);
        ok("content length present", strstr(received, "Content-Length: 2\r\n\r\nOK") != NULL);
    }

    teapot_response_free(&resp);
    close(fds[0]);
    close(fds[1]);
}

int main(void)
{
    test_long_content_type_header();
    return failures == 0 ? 0 : 1;
}
