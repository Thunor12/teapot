#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

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
static void test_long_content_type_response(void)
{
    int sockets[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
    {
        ok("socketpair created", 0);
        return;
    }

    char content_type[600];
    memset(content_type, 'a', sizeof(content_type) - 1);
    content_type[sizeof(content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "OK", 2);

    int sent = teapot_send_response((stb_teapot_socket_t)sockets[0], &resp);
    ok("send long content type response", sent == 0);
    shutdown(sockets[0], SHUT_WR);

    tp_string_builder expected = {0};
    tp_sb_appendf(
        &expected,
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: 2\r\n\r\nOK",
        content_type);

    char received[1024];
    size_t received_len = 0;
    while (received_len < sizeof(received))
    {
        ssize_t n = read(sockets[1], received + received_len, sizeof(received) - received_len);
        if (n <= 0)
        {
            break;
        }
        received_len += (size_t)n;
    }

    ok("received complete long header response", received_len == expected.count);
    ok("long header response bytes match",
       received_len == expected.count && memcmp(received, expected.items, expected.count) == 0);

    tp_sb_free(expected);
    teapot_response_free(&resp);
    close(sockets[0]);
    close(sockets[1]);
}
#else
static void test_long_content_type_response(void)
{
    printf("[SKIP] long content type response socket test is POSIX-only\n");
}
#endif

int main(void)
{
    printf("Running response unit tests...\n\n");

    test_long_content_type_response();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
