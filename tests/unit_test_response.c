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
static void test_long_content_type_response_header(void)
{
    int sockets[2] = {-1, -1};
    char content_type[512];
    char received[1024];
    size_t total = 0;
    teapot_response resp;

    memset(content_type, 'a', sizeof(content_type) - 1);
    content_type[sizeof(content_type) - 1] = '\0';

    ok("create socketpair", socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    if (sockets[0] < 0 || sockets[1] < 0)
    {
        return;
    }

    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "OK", 2);

    ok("send response with long content type", teapot_send_response((stb_teapot_socket_t)sockets[0], &resp) == 0);
    shutdown(sockets[0], SHUT_WR);

    while (total < sizeof(received) - 1)
    {
        ssize_t n = read(sockets[1], received + total, sizeof(received) - 1 - total);
        if (n <= 0)
        {
            break;
        }
        total += (size_t)n;
    }
    received[total] = '\0';

    ok("received complete long content type", strstr(received, content_type) != NULL);
    ok("received header terminator and body", strstr(received, "\r\n\r\nOK") != NULL);
    ok("content length is correct", strstr(received, "Content-Length: 2\r\n\r\nOK") != NULL);

    teapot_response_free(&resp);
    close(sockets[0]);
    close(sockets[1]);
}
#endif

int main(void)
{
    printf("Running response unit tests...\n\n");

#ifndef _WIN32
    test_long_content_type_response_header();
#else
    printf("[SKIP] socketpair response tests on Windows\n");
#endif

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
