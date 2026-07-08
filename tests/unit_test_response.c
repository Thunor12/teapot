#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <errno.h>
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

static size_t read_all(int fd, char *buf, size_t cap)
{
    size_t total = 0;
    while (total + 1 < cap)
    {
        ssize_t n = read(fd, buf + total, cap - total - 1);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }
        if (n == 0)
        {
            break;
        }
        total += (size_t)n;
    }
    buf[total] = '\0';
    return total;
}

static void test_long_content_type_is_sent_without_stack_overread(void)
{
    int sv[2];
    ok("socketpair for long content type", socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    char content_type[600];
    memset(content_type, 'a', sizeof(content_type) - 1);
    content_type[sizeof(content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "ok", 2);

    ok("send long content type response", teapot_send_response(sv[0], &resp) == 0);
    (void)shutdown(sv[0], SHUT_WR);

    char received[2048];
    size_t received_len = read_all(sv[1], received, sizeof(received));
    ok("received response bytes", received_len > 0);
    ok("full content type present", strstr(received, content_type) != NULL);
    ok("body length is exact", strstr(received, "Content-Length: 2\r\n\r\nok") != NULL);

    teapot_response_free(&resp);
    close(sv[0]);
    close(sv[1]);
}

static void test_content_type_rejects_response_splitting(void)
{
    int sv[2];
    ok("socketpair for injection test", socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = "text/plain\r\nX-Injected: yes";
    teapot_response_write(&resp, "ok", 2);

    ok("reject CRLF in content type", teapot_send_response(sv[0], &resp) < 0);

    teapot_response_free(&resp);
    close(sv[0]);
    close(sv[1]);
}

int main(void)
{
    test_long_content_type_is_sent_without_stack_overread();
    test_content_type_rejects_response_splitting();

    if (failures == 0)
    {
        printf("ALL RESPONSE TESTS PASSED\n");
        return 0;
    }

    printf("%d RESPONSE TEST(S) FAILED\n", failures);
    return 1;
}
