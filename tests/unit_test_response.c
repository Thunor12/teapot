#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <stdlib.h>
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
static void close_pair(int sockets[2])
{
    close(sockets[0]);
    close(sockets[1]);
}

static void test_long_content_type_is_sent_safely(void)
{
    int sockets[2] = {-1, -1};
    ok("socketpair for long content type", socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
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

    ok("send long content type response", teapot_send_response(sockets[0], &resp) == 0);

    char expected[1024];
    int expected_len = snprintf(
        expected, sizeof(expected),
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: 2\r\n\r\nok",
        content_type);
    ok("expected response fits test buffer", expected_len > 0 && (size_t)expected_len < sizeof(expected));

    char actual[1024] = {0};
    ssize_t received = recv(sockets[1], actual, sizeof(actual) - 1, 0);
    ok("received long content type response", received == expected_len);
    ok("long content type response bytes match",
       received == expected_len && memcmp(actual, expected, (size_t)expected_len) == 0);

    teapot_response_free(&resp);
    close_pair(sockets);
}

static void test_rejects_response_header_injection(void)
{
    int sockets[2] = {-1, -1};
    ok("socketpair for response injection", socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    if (sockets[0] < 0 || sockets[1] < 0)
    {
        return;
    }

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = "text/plain\r\nX-Injected: yes";
    teapot_response_write(&resp, "ok", 2);

    ok("reject CRLF in content type", teapot_send_response(sockets[0], &resp) < 0);

    teapot_response_free(&resp);
    close_pair(sockets);
}
#endif

int main(void)
{
#ifdef _WIN32
    printf("unit_test_response uses socketpair and is skipped on Windows.\n");
#else
    test_long_content_type_is_sent_safely();
    test_rejects_response_header_injection();
#endif

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
