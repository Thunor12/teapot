#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
int main(void)
{
    puts("unit_test_response: skipped on Windows");
    return 0;
}
#else
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
    size_t used = 0;
    while (used + 1 < cap)
    {
        ssize_t n = read(fd, buf + used, cap - used - 1);
        if (n <= 0)
        {
            break;
        }
        used += (size_t)n;
    }
    buf[used] = '\0';
    return used;
}

static void test_long_content_type_response(void)
{
    char long_content_type[600];
    memset(long_content_type, 'x', sizeof(long_content_type) - 1);
    long_content_type[sizeof(long_content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = long_content_type;
    teapot_response_write(&resp, "body", 4);

    int fds[2];
    ok("pipe created", pipe(fds) == 0);
    if (failures)
    {
        teapot_response_free(&resp);
        return;
    }

    ok("send response succeeds", teapot_send_response(fds[1], &resp) == 0);
    close(fds[1]);

    char output[1024];
    size_t output_len = read_all(fds[0], output, sizeof(output));
    close(fds[0]);

    ok("response was written", output_len > 0);
    ok("long content type is not truncated", strstr(output, long_content_type) != NULL);
    ok("body follows complete header", strstr(output, "\r\nContent-Length: 4\r\n\r\nbody") != NULL);

    teapot_response_free(&resp);
}

int main(void)
{
    test_long_content_type_response();
    return failures == 0 ? 0 : 1;
}
#endif
