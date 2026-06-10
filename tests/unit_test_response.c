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
static int read_socket_to_string(int fd, char *buf, size_t buf_size)
{
    size_t used = 0;
    while (used + 1 < buf_size)
    {
        ssize_t n = recv(fd, buf + used, buf_size - used - 1, 0);
        if (n < 0)
        {
            return 0;
        }
        if (n == 0)
        {
            break;
        }
        used += (size_t)n;
    }
    buf[used] = '\0';
    return 1;
}

static void test_long_content_type_is_serialized_safely(void)
{
    int sv[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
    {
        ok("socketpair created", 0);
        return;
    }

    char content_type[512];
    memset(content_type, 'a', sizeof(content_type) - 1);
    content_type[sizeof(content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "OK", strlen("OK"));

    ok("send long content type response", teapot_send_response(sv[0], &resp) == 0);
    shutdown(sv[0], SHUT_WR);

    char received[1024];
    ok("read long content type response", read_socket_to_string(sv[1], received, sizeof(received)));
    ok("long content type present", strstr(received, content_type) != NULL);
    ok("body present after long header", strstr(received, "\r\n\r\nOK") != NULL);

    teapot_response_free(&resp);
    close(sv[0]);
    close(sv[1]);
}

static void test_content_type_rejects_response_splitting(void)
{
    int sv[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
    {
        ok("socketpair created", 0);
        return;
    }

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = "text/plain\r\nX-Injected: yes";
    teapot_response_write(&resp, "OK", strlen("OK"));

    ok("send sanitized content type response", teapot_send_response(sv[0], &resp) == 0);
    shutdown(sv[0], SHUT_WR);

    char received[512];
    ok("read sanitized content type response", read_socket_to_string(sv[1], received, sizeof(received)));
    ok("injected header absent", strstr(received, "X-Injected") == NULL);
    ok("fallback content type used", strstr(received, "Content-Type: text/plain\r\n") != NULL);

    teapot_response_free(&resp);
    close(sv[0]);
    close(sv[1]);
}
#endif

int main(void)
{
    printf("Running response unit tests...\n\n");

#ifndef _WIN32
    test_long_content_type_is_serialized_safely();
    test_content_type_rejects_response_splitting();
#else
    printf("socketpair-based response tests skipped on Windows\n");
#endif

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
