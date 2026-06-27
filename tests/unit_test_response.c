#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
int main(void)
{
    puts("unit_test_response uses socketpair; skipping on Windows");
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

static int make_socket_pair(int sockets[2])
{
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
    {
        perror("socketpair");
        return -1;
    }
    return 0;
}

static ssize_t read_response(int fd, char *buf, size_t cap)
{
    ssize_t n = recv(fd, buf, cap - 1, 0);
    if (n < 0)
    {
        perror("recv");
        return -1;
    }
    buf[(size_t)n] = '\0';
    return n;
}

static void close_pair(int sockets[2])
{
    close(sockets[0]);
    close(sockets[1]);
}

static void test_long_content_type_is_sent_completely(void)
{
    int sockets[2];
    if (make_socket_pair(sockets) != 0)
    {
        ++failures;
        return;
    }

    char content_type[600];
    memset(content_type, 'x', sizeof(content_type));
    memcpy(content_type, "application/", strlen("application/"));
    content_type[sizeof(content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "ok", 2);

    ok("send long content-type response", teapot_send_response(sockets[0], &resp) == 0);

    char buf[2048];
    ssize_t n = read_response(sockets[1], buf, sizeof(buf));
    ok("read long content-type response", n > 0);
    ok("long content-type is present", strstr(buf, content_type) != NULL);
    ok("long response header is complete", strstr(buf, "\r\n\r\nok") != NULL);

    teapot_response_free(&resp);
    close_pair(sockets);
}

static void test_content_type_rejects_response_splitting(void)
{
    int sockets[2];
    if (make_socket_pair(sockets) != 0)
    {
        ++failures;
        return;
    }

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = "text/plain\r\nX-Injected: yes";
    teapot_response_write(&resp, "ok", 2);

    ok("send sanitized content-type response", teapot_send_response(sockets[0], &resp) == 0);

    char buf[512];
    ssize_t n = read_response(sockets[1], buf, sizeof(buf));
    ok("read sanitized content-type response", n > 0);
    ok("unsafe content-type falls back", strstr(buf, "Content-Type: text/plain\r\n") != NULL);
    ok("injected header is absent", strstr(buf, "X-Injected: yes") == NULL);

    teapot_response_free(&resp);
    close_pair(sockets);
}

int main(void)
{
    test_long_content_type_is_sent_completely();
    test_content_type_rejects_response_splitting();

    if (failures == 0)
    {
        puts("ALL RESPONSE TESTS PASSED");
        return 0;
    }

    printf("%d RESPONSE TEST(S) FAILED\n", failures);
    return 1;
}
#endif
