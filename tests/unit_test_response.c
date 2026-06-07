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

static size_t read_available(int fd, char *buf, size_t cap)
{
    size_t total = 0;
    while (total + 1 < cap)
    {
        ssize_t n = recv(fd, buf + total, cap - total - 1, 0);
        if (n <= 0)
        {
            break;
        }
        total += (size_t)n;
    }
    buf[total] = '\0';
    return total;
}

static void test_long_content_type_response(void)
{
    int sv[2] = {-1, -1};
    ok("socketpair for long response", socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
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

    ok("send long content-type response", teapot_send_response(sv[0], &resp) == 0);
    shutdown(sv[0], SHUT_WR);

    char wire[2048];
    size_t wire_len = read_available(sv[1], wire, sizeof(wire));
    ok("wire response was read", wire_len > 0);
    ok("long content type present", strstr(wire, content_type) != NULL);
    ok("content length preserved", strstr(wire, "Content-Length: 2\r\n\r\nok") != NULL);

    teapot_response_free(&resp);
    close(sv[0]);
    close(sv[1]);
}

static void test_content_type_crlf_rejected(void)
{
    int sv[2] = {-1, -1};
    ok("socketpair for injection response", socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    if (sv[0] < 0 || sv[1] < 0)
    {
        return;
    }

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = "text/plain\r\nX-Injected: yes";
    teapot_response_write(&resp, "ok", 2);

    ok("response splitting content type rejected", teapot_send_response(sv[0], &resp) < 0);

    teapot_response_free(&resp);
    close(sv[0]);
    close(sv[1]);
}

int main(void)
{
    printf("Running response unit tests...\n\n");
    test_long_content_type_response();
    test_content_type_crlf_rejected();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
