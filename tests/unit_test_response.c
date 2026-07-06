#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
int main(void)
{
    printf("unit_test_response is skipped on Windows\n");
    return 0;
}
#else
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

static void test_long_content_type_is_sent_completely(void)
{
    int sv[2] = {-1, -1};
    ok("socketpair", socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    if (sv[0] < 0 || sv[1] < 0)
    {
        return;
    }

    char content_type[600];
    memset(content_type, 'a', sizeof(content_type));
    memcpy(content_type, "text/plain; charset=", strlen("text/plain; charset="));
    content_type[sizeof(content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "OK", 2);

    ok("send response with long content type", teapot_send_response(sv[0], &resp) == 0);
    shutdown(sv[0], SHUT_WR);

    char received[2048];
    size_t total = 0;
    while (total < sizeof(received) - 1)
    {
        ssize_t n = read(sv[1], received + total, sizeof(received) - 1 - total);
        if (n <= 0)
        {
            break;
        }
        total += (size_t)n;
    }
    received[total] = '\0';

    ok("status line present", strstr(received, "HTTP/1.1 200 OK\r\n") != NULL);
    ok("complete content type present", strstr(received, content_type) != NULL);
    ok("content length present", strstr(received, "Content-Length: 2\r\n\r\n") != NULL);
    ok("body present", total >= 2 && memcmp(received + total - 2, "OK", 2) == 0);

    teapot_response_free(&resp);
    close(sv[0]);
    close(sv[1]);
}

int main(void)
{
    printf("Running response unit tests...\n\n");

    test_long_content_type_is_sent_completely();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
#endif
