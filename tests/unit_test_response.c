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

    char content_type[400];
    memset(content_type, 'a', sizeof(content_type) - 1);
    content_type[sizeof(content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    tp_sb_append_cstr(&resp.body, "ok");

    char expected[512];
    int expected_header_len = snprintf(
        expected, sizeof(expected),
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: 2\r\n\r\nok",
        content_type);
    ok("expected response fits fixture", expected_header_len > 0 && (size_t)expected_header_len < sizeof(expected));

    ok("send response succeeds", teapot_send_response((stb_teapot_socket_t)sockets[0], &resp) == 0);
    teapot_close((stb_teapot_socket_t)sockets[0]);
    sockets[0] = -1;

    char received[512] = {0};
    size_t received_len = 0;
    while (received_len < sizeof(received) - 1)
    {
        int n = teapot_read((stb_teapot_socket_t)sockets[1], received + received_len, (int)(sizeof(received) - 1 - received_len));
        if (n <= 0)
        {
            break;
        }
        received_len += (size_t)n;
    }
    teapot_close((stb_teapot_socket_t)sockets[1]);
    sockets[1] = -1;

    if (expected_header_len > 0)
    {
        size_t expected_len = (size_t)expected_header_len;
        ok("received full response length", received_len == expected_len);
        ok("long content type header is intact",
           received_len == expected_len && memcmp(received, expected, expected_len) == 0);
    }

    teapot_response_free(&resp);
}

int main(void)
{
    test_long_content_type_header();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
#endif
