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
static char *read_socket_until_eof(int fd, size_t *out_len)
{
    tp_string_builder sb = {0};
    char buf[256];
    for (;;)
    {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0)
        {
            tp_sb_free(sb);
            return NULL;
        }
        if (n == 0)
        {
            break;
        }
        tp_sb_append_buf(&sb, buf, (size_t)n);
    }

    tp_sb_append_null(&sb);
    if (out_len)
    {
        *out_len = sb.count - 1;
    }
    return sb.items;
}

static void test_long_content_type_response(void)
{
    int fds[2] = {-1, -1};
    ok("socketpair created", socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    if (fds[0] < 0 || fds[1] < 0)
    {
        return;
    }

    char content_type[320];
    memset(content_type, 'a', sizeof(content_type) - 1);
    content_type[sizeof(content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "hello", 5);

    ok("send long content type response", teapot_send_response(fds[0], &resp) == 0);
    shutdown(fds[0], SHUT_WR);

    size_t got_len = 0;
    char *got = read_socket_until_eof(fds[1], &got_len);

    int expected_len = snprintf(
        NULL, 0,
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: 5\r\n\r\nhello",
        content_type);
    char *expected = malloc((size_t)expected_len + 1);
    snprintf(
        expected, (size_t)expected_len + 1,
        "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: 5\r\n\r\nhello",
        content_type);

    ok("long content type response length is exact", got && got_len == (size_t)expected_len);
    ok("long content type response bytes are exact", got && memcmp(got, expected, (size_t)expected_len) == 0);

    free(expected);
    free(got);
    teapot_response_free(&resp);
    close(fds[0]);
    close(fds[1]);
}
#endif

int main(void)
{
    printf("Running response unit tests...\n\n");

#ifndef _WIN32
    test_long_content_type_response();
#else
    printf("[SKIP] response socket tests require POSIX socketpair\n");
#endif

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
