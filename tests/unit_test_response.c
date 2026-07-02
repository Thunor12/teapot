#define _POSIX_C_SOURCE 200809L
#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
int main(void)
{
    return 0;
}
#else
#include <sys/socket.h>
#include <unistd.h>

static int failures = 0;

static void ok(const char *name, int cond)
{
    if (cond)
        printf("[PASS] %s\n", name);
    else
    {
        printf("[FAIL] %s\n", name);
        ++failures;
    }
}

static void close_pair(int fds[2])
{
    close(fds[0]);
    close(fds[1]);
}

static void test_long_content_type(void)
{
    int fds[2] = {-1, -1};
    ok("socketpair for long content type", socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    if (fds[0] < 0 || fds[1] < 0)
        return;

    char content_type[600];
    memset(content_type, 'a', sizeof(content_type) - 1);
    content_type[sizeof(content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "hello", 5);

    ok("send long content type", teapot_send_response(fds[0], &resp) == 0);
    char out[1024] = {0};
    ssize_t n = read(fds[1], out, sizeof(out) - 1);
    ok("read long content type response", n > 0);
    ok("response contains complete content type", strstr(out, content_type) != NULL);
    ok("response contains body", strstr(out, "\r\n\r\nhello") != NULL);

    teapot_response_free(&resp);
    close_pair(fds);
}

static void test_reject_header_injection(void)
{
    int fds[2] = {-1, -1};
    ok("socketpair for injection test", socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    if (fds[0] < 0 || fds[1] < 0)
        return;

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = "text/plain\r\nX-Injected: yes";
    teapot_response_write(&resp, "hello", 5);

    ok("reject CRLF in content type", teapot_send_response(fds[0], &resp) == -1);

    teapot_response_free(&resp);
    close_pair(fds);
}

int main(void)
{
    test_long_content_type();
    test_reject_header_injection();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
#endif
