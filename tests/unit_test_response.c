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

static ssize_t read_all(int fd, char *buf, size_t cap)
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
    return (ssize_t)total;
}

static void test_long_content_type_is_sent_safely(void)
{
    int sv[2] = {-1, -1};
    ok("socketpair for long content type", socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    if (sv[0] < 0 || sv[1] < 0)
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

    ok("send long content type response", teapot_send_response((stb_teapot_socket_t)sv[0], &resp) == 0);
    shutdown(sv[0], SHUT_WR);

    char received[1024];
    ssize_t n = read_all(sv[1], received, sizeof(received));
    ok("read long content type response", n > 0);

    char expected_header[800];
    int expected_header_len = snprintf(
        expected_header, sizeof(expected_header),
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: 2\r\n\r\n", content_type);
    ok("expected header fits test buffer", expected_header_len > 0 && (size_t)expected_header_len < sizeof(expected_header));
    ok("response length is exact", n == (ssize_t)((size_t)expected_header_len + 2));
    ok("response header is not truncated", strncmp(received, expected_header, (size_t)expected_header_len) == 0);
    ok("response body follows header", n >= 2 && memcmp(received + n - 2, "ok", 2) == 0);

    teapot_response_free(&resp);
    close(sv[0]);
    close(sv[1]);
}

static void test_content_type_rejects_crlf(void)
{
    int sv[2] = {-1, -1};
    ok("socketpair for CRLF content type", socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    if (sv[0] < 0 || sv[1] < 0)
    {
        return;
    }

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = "text/plain\r\nX-Injected: yes";
    teapot_response_write(&resp, "bad", 3);

    ok("reject response header injection", teapot_send_response((stb_teapot_socket_t)sv[0], &resp) < 0);

    teapot_response_free(&resp);
    close(sv[0]);
    close(sv[1]);
}

int main(void)
{
    printf("Running response unit tests...\n\n");

    test_long_content_type_is_sent_safely();
    test_content_type_rejects_crlf();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
