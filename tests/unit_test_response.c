#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#else
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

static void close_test_socket(stb_teapot_socket_t s)
{
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

static int make_socket_pair(stb_teapot_socket_t sockets[2])
{
#ifdef _WIN32
    (void)sockets;
    return -1;
#else
    return socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
#endif
}

static size_t read_all(stb_teapot_socket_t s, char *buf, size_t cap)
{
    size_t total = 0;
    while (total < cap)
    {
#ifdef _WIN32
        int n = recv(s, buf + total, (int)(cap - total), 0);
#else
        ssize_t n = read(s, buf + total, cap - total);
#endif
        if (n <= 0)
        {
            break;
        }
        total += (size_t)n;
    }
    return total;
}

static void test_long_content_type_is_not_truncated_or_overread(void)
{
    stb_teapot_socket_t sockets[2];
    if (make_socket_pair(sockets) != 0)
    {
        printf("[SKIP] socketpair unavailable\n");
        return;
    }

    char content_type[384];
    memset(content_type, 'a', sizeof(content_type) - 1);
    content_type[sizeof(content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "ok", 2);

    int send_result = teapot_send_response(sockets[0], &resp);
    close_test_socket(sockets[0]);

    char actual[1024] = {0};
    size_t actual_len = read_all(sockets[1], actual, sizeof(actual) - 1);
    close_test_socket(sockets[1]);

    char expected[1024] = {0};
    int header_len = snprintf(
        expected, sizeof(expected),
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: 2\r\n\r\nok",
        content_type);

    ok("long content type send succeeds", send_result == 0);
    ok("expected response fits test buffer", header_len > 0 && (size_t)header_len < sizeof(expected));
    ok("long content type response length is exact", actual_len == (size_t)header_len);
    ok("long content type response bytes are exact", memcmp(actual, expected, actual_len) == 0);

    teapot_response_free(&resp);
}

static void test_content_type_rejects_crlf(void)
{
    stb_teapot_socket_t sockets[2];
    if (make_socket_pair(sockets) != 0)
    {
        printf("[SKIP] socketpair unavailable\n");
        return;
    }

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = "text/plain\r\nX-Injected: yes";
    teapot_response_write(&resp, "ok", 2);

    ok("CRLF content type is rejected", teapot_send_response(sockets[0], &resp) == -1);

    close_test_socket(sockets[0]);
    close_test_socket(sockets[1]);
    teapot_response_free(&resp);
}

int main(void)
{
    printf("Running response unit tests...\n\n");

    test_long_content_type_is_not_truncated_or_overread();
    test_content_type_rejects_crlf();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
