#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void ok(const char *name, int cond)
{
    if (cond)
        printf("[PASS] %s\n", name);
    else
    {
        printf("[FAIL] %s\n", name);
        ++failures;
    }
}

static int make_socket_pair(stb_teapot_socket_t sv[2])
{
#ifdef _WIN32
    (void)sv;
    return -1;
#else
    return socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
#endif
}

static void close_socket(stb_teapot_socket_t s)
{
    teapot_close(s);
}

static void test_long_content_type_does_not_truncate_or_overread(void)
{
    stb_teapot_socket_t sv[2];
    teapot_response resp;
    char content_type[400];
    char received[1024];

    memset(content_type, 'a', sizeof(content_type) - 1);
    content_type[0] = 't';
    content_type[1] = 'e';
    content_type[2] = 'x';
    content_type[3] = 't';
    content_type[4] = '/';
    content_type[sizeof(content_type) - 1] = '\0';

    if (make_socket_pair(sv) != 0)
    {
        ok("socket pair created", 0);
        return;
    }

    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "OK", 2);

    ok("long content-type response sends", teapot_send_response(sv[0], &resp) == 0);
    ssize_t n = recv(sv[1], received, sizeof(received) - 1, 0);
    ok("response bytes received", n > 0);
    if (n > 0)
    {
        size_t received_len = (size_t)n;
        received[received_len] = '\0';
        ok("full content-type present", strstr(received, content_type) != NULL);
        ok("content length is body length", strstr(received, "Content-Length: 2\r\n") != NULL);
        ok("body bytes present", strstr(received, "\r\n\r\nOK") != NULL);
    }

    teapot_response_free(&resp);
    close_socket(sv[0]);
    close_socket(sv[1]);
}

static void test_content_type_rejects_response_splitting(void)
{
    stb_teapot_socket_t sv[2];
    teapot_response resp;

    if (make_socket_pair(sv) != 0)
    {
        ok("socket pair created", 0);
        return;
    }

    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = "text/plain\r\nX-Injected: yes";
    teapot_response_write(&resp, "OK", 2);

    ok("crlf content-type rejected", teapot_send_response(sv[0], &resp) < 0);

    teapot_response_free(&resp);
    close_socket(sv[0]);
    close_socket(sv[1]);
}

int main(void)
{
    printf("Running response unit tests...\n\n");

    test_long_content_type_does_not_truncate_or_overread();
    test_content_type_rejects_response_splitting();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
