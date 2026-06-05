#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#ifdef _WIN32
#include <stdio.h>

int main(void)
{
    printf("unit_test_response is skipped on Windows because it uses socketpair.\n");
    return 0;
}

#else
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

static void read_available(int fd, char *buffer, size_t buffer_size)
{
    size_t total = 0;
    while (total + 1 < buffer_size)
    {
        ssize_t n = read(fd, buffer + total, buffer_size - total - 1);
        if (n <= 0)
        {
            break;
        }
        total += (size_t)n;
    }
    buffer[total] = '\0';
}

static void test_long_content_type_response(void)
{
    int sockets[2] = {-1, -1};
    ok("socketpair for long content-type", socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    if (sockets[0] < 0 || sockets[1] < 0)
    {
        return;
    }

    char content_type[600];
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
    tp_sb_append_cstr(&resp.body, "OK");

    ok("long content-type send succeeds", teapot_send_response((stb_teapot_socket_t)sockets[1], &resp) == 0);
    shutdown(sockets[1], SHUT_WR);

    char response[1024];
    read_available(sockets[0], response, sizeof(response));
    ok("long content-type status sent", strstr(response, "HTTP/1.1 200 OK") != NULL);
    ok("long content-type value sent", strstr(response, content_type) != NULL);
    ok("long content-type body sent", strstr(response, "\r\n\r\nOK") != NULL);

    teapot_response_free(&resp);
    close(sockets[0]);
    close(sockets[1]);
}

static void test_content_type_rejects_crlf(void)
{
    int sockets[2] = {-1, -1};
    ok("socketpair for crlf content-type", socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    if (sockets[0] < 0 || sockets[1] < 0)
    {
        return;
    }

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = "text/plain\r\nX-Injected: yes";
    tp_sb_append_cstr(&resp.body, "OK");

    ok("crlf content-type rejected", teapot_send_response((stb_teapot_socket_t)sockets[1], &resp) == -1);

    teapot_response_free(&resp);
    close(sockets[0]);
    close(sockets[1]);
}

int main(void)
{
    test_long_content_type_response();
    test_content_type_rejects_crlf();

    if (failures == 0)
    {
        printf("\nALL RESPONSE TESTS PASSED\n");
        return 0;
    }

    printf("\n%d RESPONSE TEST(S) FAILED\n", failures);
    return 1;
}
#endif
