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
    teapot_response_write(&resp, "ok", 2);

    ok("send response succeeds", teapot_send_response(sockets[0], &resp) == 0);
    shutdown(sockets[0], SHUT_WR);

    char received[1024];
    ssize_t n = read(sockets[1], received, sizeof(received) - 1);
    ok("response bytes received", n > 0);
    if (n > 0)
    {
        received[n] = '\0';
        ok("status line present", strstr(received, "HTTP/1.1 200 OK\r\n") != NULL);
        ok("full content type present", strstr(received, content_type) != NULL);
        ok("body present", strstr(received, "\r\n\r\nok") != NULL);
    }

    teapot_response_free(&resp);
    close(sockets[0]);
    close(sockets[1]);
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
