#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
int main(void)
{
    puts("unit_test_response skipped on Windows");
    return 0;
}
#else
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

static void test_long_content_type_response(void)
{
    int fds[2] = {-1, -1};
    ok("socketpair created", socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    if (fds[0] < 0 || fds[1] < 0)
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

    ok("send long content-type response", teapot_send_response((stb_teapot_socket_t)fds[0], &resp) == 0);

    char received[1024];
    ssize_t n = read(fds[1], received, sizeof(received) - 1);
    ok("received response bytes", n > 0);
    if (n > 0)
    {
        received[(size_t)n] = '\0';
        ok("long content-type preserved", strstr(received, content_type) != NULL);
        ok("content length preserved", strstr(received, "Content-Length: 2\r\n\r\nOK") != NULL);
    }

    teapot_response_free(&resp);
    close(fds[0]);
    close(fds[1]);
}

int main(void)
{
    test_long_content_type_response();

    if (failures == 0)
    {
        puts("ALL TESTS PASSED");
        return 0;
    }

    printf("%d TEST(S) FAILED\n", failures);
    return 1;
}
#endif
