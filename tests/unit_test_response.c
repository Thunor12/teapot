#define _POSIX_C_SOURCE 200809L
#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
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
static size_t read_available_response(int fd, char *buf, size_t cap)
{
    size_t total = 0;
    while (total + 1 < cap)
    {
        ssize_t n = read(fd, buf + total, cap - total - 1);
        if (n <= 0)
        {
            break;
        }
        total += (size_t)n;
    }
    buf[total] = '\0';
    return total;
}

static void test_long_content_type_is_not_truncated_or_overread(void)
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
    shutdown(sv[0], SHUT_WR);

    char received[2048];
    read_available_response(sv[1], received, sizeof(received));
    ok("long content type preserved", strstr(received, content_type) != NULL);
    ok("content length still present", strstr(received, "Content-Length: 2\r\n\r\nok") != NULL);

    teapot_response_free(&resp);
    close(sv[0]);
    close(sv[1]);
}

static void test_content_type_crlf_is_sanitized(void)
{
    int sv[2];
    ok("socketpair for crlf content type", socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = "text/plain\r\nX-Injected: yes";
    teapot_response_write(&resp, "ok", 2);

    ok("send sanitized content type response", teapot_send_response(sv[0], &resp) == 0);
    shutdown(sv[0], SHUT_WR);

    char received[512];
    read_available_response(sv[1], received, sizeof(received));
    ok("injected header absent", strstr(received, "X-Injected") == NULL);
    ok("fallback content type used", strstr(received, "Content-Type: text/plain\r\n") != NULL);

    teapot_response_free(&resp);
    close(sv[0]);
    close(sv[1]);
}
#endif

int main(void)
{
#ifdef _WIN32
    printf("unit_test_response is POSIX-only for socketpair coverage\n");
#else
    test_long_content_type_is_not_truncated_or_overread();
    test_content_type_crlf_is_sanitized();
#endif

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
