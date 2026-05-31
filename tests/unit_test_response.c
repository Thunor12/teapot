#ifndef _WIN32
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

static size_t read_all(int fd, char *buf, size_t cap)
{
    size_t total = 0;
    while (total < cap)
    {
        ssize_t n = read(fd, buf + total, cap - total);
        if (n <= 0)
        {
            break;
        }
        total += (size_t)n;
    }
    return total;
}

static void test_long_content_type_response_header(void)
{
    int sockets[2] = {-1, -1};
    ok("create socketpair", socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    if (sockets[0] < 0 || sockets[1] < 0)
    {
        return;
    }

    char content_type[401];
    memset(content_type, 'a', sizeof(content_type) - 1);
    content_type[0] = 't';
    content_type[1] = 'e';
    content_type[2] = 'x';
    content_type[3] = 't';
    content_type[4] = '/';
    content_type[sizeof(content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "OK", 2);

    ok("send response with long content type", teapot_send_response(sockets[0], &resp) == 0);
    shutdown(sockets[0], SHUT_WR);

    char received[1024];
    size_t received_len = read_all(sockets[1], received, sizeof(received) - 1);
    received[received_len] = '\0';

    char expected_header[512];
    snprintf(expected_header, sizeof(expected_header), "Content-Type: %s\r\n", content_type);
    ok("long content type is serialized completely", strstr(received, expected_header) != NULL);
    ok("content length is serialized", strstr(received, "Content-Length: 2\r\n\r\n") != NULL);
    ok("body is serialized after headers", received_len >= 2 && memcmp(received + received_len - 2, "OK", 2) == 0);

    teapot_response_free(&resp);
    close(sockets[0]);
    close(sockets[1]);
}

int main(void)
{
    test_long_content_type_response_header();

    if (failures == 0)
    {
        printf("ALL RESPONSE TESTS PASSED\n");
        return 0;
    }

    printf("%d RESPONSE TEST(S) FAILED\n", failures);
    return 1;
}
#else
int main(void)
{
    return 0;
}
#endif
