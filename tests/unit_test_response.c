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
static ssize_t read_all(int fd, char *buf, size_t buf_size)
{
    size_t used = 0;
    while (used + 1u < buf_size)
    {
        ssize_t n = read(fd, buf + used, buf_size - used - 1u);
        if (n <= 0)
        {
            break;
        }
        used += (size_t)n;
    }
    buf[used] = '\0';
    return (ssize_t)used;
}

static void test_long_content_type_response(void)
{
    int sv[2] = {-1, -1};
    ok("socketpair created", socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    if (sv[0] < 0 || sv[1] < 0)
    {
        return;
    }

    char content_type[600];
    memset(content_type, 'a', sizeof(content_type) - 1u);
    content_type[sizeof(content_type) - 1u] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "OK", 2u);

    ok("send response succeeds", teapot_send_response((stb_teapot_socket_t)sv[0], &resp) == 0);
    shutdown(sv[0], SHUT_WR);

    char received[1024];
    ssize_t n = read_all(sv[1], received, sizeof(received));
    ok("received response bytes", n > 0);
    ok("long content type preserved", strstr(received, content_type) != NULL);
    ok("content length preserved", strstr(received, "Content-Length: 2\r\n\r\nOK") != NULL);

    teapot_response_free(&resp);
    close(sv[0]);
    close(sv[1]);
}
#endif

int main(void)
{
    printf("Running response unit tests...\n\n");

#ifndef _WIN32
    test_long_content_type_response();
#else
    printf("[SKIP] socketpair-based tests are POSIX-only\n");
#endif

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
