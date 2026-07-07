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

static int make_socket_pair(int sv[2])
{
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
    {
        perror("socketpair");
        return 0;
    }
    return 1;
}

static size_t read_response(int fd, char *buf, size_t cap)
{
    size_t total = 0;
    while (total + 1 < cap)
    {
        ssize_t n = read(fd, buf + total, cap - total - 1);
        if (n <= 0)
        {
            break;
        }
        total += (size_t)n;
    }
    buf[total] = '\0';
    return total;
}

static void test_long_content_type_is_not_truncated(void)
{
    int sv[2] = {-1, -1};
    if (!make_socket_pair(sv))
    {
        ++failures;
        return;
    }

    char long_content_type[400];
    memset(long_content_type, 'a', sizeof(long_content_type) - 1);
    long_content_type[sizeof(long_content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = long_content_type;
    teapot_response_write(&resp, "ok", 2);

    ok("long content type send succeeds", teapot_send_response(sv[0], &resp) == 0);
    shutdown(sv[0], SHUT_WR);

    char response[1024];
    read_response(sv[1], response, sizeof(response));
    ok("long content type appears in response", strstr(response, long_content_type) != NULL);
    ok("body length remains correct", strstr(response, "Content-Length: 2\r\n\r\nok") != NULL);

    teapot_response_free(&resp);
    close(sv[0]);
    close(sv[1]);
}

static void test_content_type_rejects_crlf(void)
{
    int sv[2] = {-1, -1};
    if (!make_socket_pair(sv))
    {
        ++failures;
        return;
    }

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = "text/plain\r\nSet-Cookie: session=attacker";
    teapot_response_write(&resp, "ok", 2);

    ok("CRLF content type is rejected", teapot_send_response(sv[0], &resp) == -1);

    teapot_response_free(&resp);
    close(sv[0]);
    close(sv[1]);
}

int main(void)
{
    test_long_content_type_is_not_truncated();
    test_content_type_rejects_crlf();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
