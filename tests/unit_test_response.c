#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <unistd.h>
#endif

static int failures = 0;

static void ok(const char *name, int condition)
{
    if (condition)
    {
        printf("[PASS] %s\n", name);
    }
    else
    {
        printf("[FAIL] %s\n", name);
        failures++;
    }
}

#ifndef _WIN32
static void test_long_content_type_response(void)
{
    int sockets[2] = {-1, -1};
    ok("socketpair created", socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    if (sockets[0] < 0 || sockets[1] < 0)
    {
        return;
    }

    char content_type[600];
    memset(content_type, 'a', sizeof(content_type) - 1);
    content_type[sizeof(content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "OK", 2);

    ok("send response succeeds", teapot_send_response((stb_teapot_socket_t)sockets[0], &resp) == 0);
    shutdown(sockets[0], SHUT_WR);

    char received[1024];
    ssize_t n = recv(sockets[1], received, sizeof(received) - 1, 0);
    ok("response bytes received", n > 0);
    if (n > 0)
    {
        received[(size_t)n] = '\0';
        ok("long content type is not truncated", strstr(received, content_type) != NULL);
        ok("body follows response headers", strstr(received, "\r\n\r\nOK") != NULL);
    }

    teapot_response_free(&resp);
    close(sockets[0]);
    close(sockets[1]);
}
#endif

int main(void)
{
    printf("Running response unit tests...\n\n");

#ifndef _WIN32
    test_long_content_type_response();
#else
    printf("[SKIP] response socket tests are POSIX-only\n");
#endif

    if (failures)
    {
        printf("\n%d response test(s) failed\n", failures);
        return 1;
    }

    printf("\nAll response tests passed.\n");
    return 0;
}
