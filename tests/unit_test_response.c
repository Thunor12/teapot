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

static size_t read_socket(stb_teapot_socket_t s, char *buf, size_t cap)
{
    size_t total = 0;
    while (total + 1 < cap)
    {
        ssize_t n = read(s, buf + total, cap - total - 1);
        if (n <= 0)
        {
            break;
        }
        total += (size_t)n;
    }
    buf[total] = '\0';
    return total;
}

static void test_long_content_type_is_sent_safely(void)
{
    stb_teapot_socket_t fds[2];
    ok("socketpair for long content-type", socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    char content_type[512];
    memset(content_type, 'a', sizeof(content_type) - 1);
    content_type[sizeof(content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "OK", 2);

    ok("send response with long content-type", teapot_send_response(fds[0], &resp) == 0);
    shutdown(fds[0], SHUT_WR);

    char buf[2048];
    size_t got = read_socket(fds[1], buf, sizeof(buf));
    ok("read response bytes", got > 0);
    ok("long content-type present", strstr(buf, content_type) != NULL);
    ok("body follows complete header", strstr(buf, "\r\n\r\nOK") != NULL);
    ok("content length is correct", strstr(buf, "Content-Length: 2\r\n") != NULL);

    close(fds[0]);
    close(fds[1]);
    teapot_response_free(&resp);
}

static void test_rejects_header_injection_content_type(void)
{
    stb_teapot_socket_t fds[2];
    ok("socketpair for injected content-type", socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = "text/plain\r\nX-Injected: yes";
    teapot_response_write(&resp, "OK", 2);

    ok("reject injected content-type", teapot_send_response(fds[0], &resp) == -1);

    close(fds[0]);
    close(fds[1]);
    teapot_response_free(&resp);
}

int main(void)
{
    test_long_content_type_is_sent_safely();
    test_rejects_header_injection_content_type();

    if (failures == 0)
    {
        printf("ALL RESPONSE TESTS PASSED\n");
        return 0;
    }

    printf("%d RESPONSE TEST(S) FAILED\n", failures);
    return 1;
}
