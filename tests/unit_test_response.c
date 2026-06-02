#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <sys/socket.h>
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
static void test_long_content_type_response(void)
{
    int fds[2];
    char content_type[384];
    char received[1024];
    size_t received_len = 0;
    ssize_t nread;
    int pair_ok;

    memset(content_type, 'x', sizeof(content_type) - 1u);
    content_type[sizeof(content_type) - 1u] = '\0';

    pair_ok = socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0;
    ok("socketpair for response", pair_ok);
    if (!pair_ok)
    {
        return;
    }

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    tp_sb_append_cstr(&resp.body, "OK");

    ok("send response with long content type", teapot_send_response((stb_teapot_socket_t)fds[0], &resp) == 0);
    shutdown(fds[0], SHUT_WR);

    while ((nread = recv(fds[1], received + received_len, sizeof(received) - received_len - 1u, 0)) > 0)
    {
        received_len += (size_t)nread;
    }
    received[received_len] = '\0';

    ok("received response bytes", received_len > 0);
    ok("response includes full long content type", strstr(received, content_type) != NULL);
    ok("response body length excludes trailing null", strstr(received, "Content-Length: 2\r\n\r\nOK") != NULL);

    teapot_response_free(&resp);
    teapot_close((stb_teapot_socket_t)fds[0]);
    teapot_close((stb_teapot_socket_t)fds[1]);
}
#endif

int main(void)
{
    printf("Running response unit tests...\n\n");

#ifndef _WIN32
    test_long_content_type_response();
#endif

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
