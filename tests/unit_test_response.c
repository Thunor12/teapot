#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#ifndef _WIN32
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

static void test_long_content_type_is_sent_safely(void)
{
    int sv[2];
    ok("socketpair for long content type", socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    char content_type[600];
    memset(content_type, 'a', sizeof(content_type) - 1);
    content_type[sizeof(content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "hello", 5);

    ok("send long content type", teapot_send_response(sv[0], &resp) == 0);
    shutdown(sv[0], SHUT_WR);

    char buf[1024];
    read_all(sv[1], buf, sizeof(buf));
    ok("long content type present", strstr(buf, content_type) != NULL);
    ok("body present", strstr(buf, "\r\n\r\nhello") != NULL);

    teapot_response_free(&resp);
    close(sv[0]);
    close(sv[1]);
}

static void test_rejects_response_header_injection(void)
{
    int sv[2];
    ok("socketpair for injected content type", socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = "text/plain\r\nX-Injected: yes";
    teapot_response_write(&resp, "hello", 5);

    ok("reject injected content type", teapot_send_response(sv[0], &resp) < 0);

    teapot_response_free(&resp);
    close(sv[0]);
    close(sv[1]);
}

int main(void)
{
    test_long_content_type_is_sent_safely();
    test_rejects_response_header_injection();

    if (failures != 0)
    {
        printf("%d response test(s) failed\n", failures);
        return 1;
    }

    printf("response tests passed\n");
    return 0;
}
#else
int main(void)
{
    return 0;
}
#endif
