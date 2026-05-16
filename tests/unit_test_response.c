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

static void test_long_content_type_header(void)
{
#ifdef _WIN32
    printf("[SKIP] long content type response header (socketpair unavailable)\n");
#else
    int sockets[2] = {-1, -1};
    ok("socketpair created", socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    if (sockets[0] < 0 || sockets[1] < 0)
    {
        return;
    }

    char content_type[520];
    memcpy(content_type, "text/", 5);
    memset(content_type + 5, 'a', sizeof(content_type) - 6);
    content_type[sizeof(content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "OK", 2);

    ok("send response succeeds", teapot_send_response(sockets[0], &resp) == 0);
    shutdown(sockets[0], SHUT_WR);

    char received[2048];
    size_t total = 0;
    for (;;)
    {
        ssize_t n = read(sockets[1], received + total, sizeof(received) - 1 - total);
        if (n <= 0)
        {
            break;
        }
        total += (size_t)n;
        if (total >= sizeof(received) - 1)
        {
            break;
        }
    }
    received[total] = '\0';

    ok("header exceeds old fixed buffer size", total > 256);
    ok("full content type is present", strstr(received, content_type) != NULL);
    ok("content length is correct", strstr(received, "Content-Length: 2\r\n\r\nOK") != NULL);

    teapot_response_free(&resp);
    close(sockets[0]);
    close(sockets[1]);
#endif
}

int main(void)
{
    printf("Running response unit tests...\n\n");

    test_long_content_type_header();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
