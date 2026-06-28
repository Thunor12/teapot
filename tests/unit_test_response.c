#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#ifdef _WIN32
int main(void)
{
    printf("Skipping socketpair response tests on Windows.\n");
    return 0;
}
#else

#include <sys/socket.h>
#include <unistd.h>

static void close_pair(int sockets[2])
{
    close(sockets[0]);
    close(sockets[1]);
}

static int read_all(int fd, char *buffer, size_t capacity, size_t *out_len)
{
    size_t count = 0;
    while (count + 1 < capacity)
    {
        ssize_t n = recv(fd, buffer + count, capacity - count - 1, 0);
        if (n < 0)
        {
            perror("recv");
            return -1;
        }
        if (n == 0)
        {
            break;
        }
        count += (size_t)n;
    }

    buffer[count] = '\0';
    *out_len = count;
    return 0;
}

static void test_long_content_type_is_sent_without_stack_overread(void)
{
    int sockets[2] = {-1, -1};
    ok("socketpair for long content type", socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    if (sockets[0] < 0 || sockets[1] < 0)
    {
        return;
    }

    char content_type[512];
    memset(content_type, 'a', sizeof(content_type) - 1);
    content_type[sizeof(content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "ok", 2);

    ok("send long content type response", teapot_send_response((stb_teapot_socket_t)sockets[0], &resp) == 0);
    shutdown(sockets[0], SHUT_WR);

    char actual[1024];
    size_t actual_len = 0;
    ok("read long content type response", read_all(sockets[1], actual, sizeof(actual), &actual_len) == 0);

    int expected_len = snprintf(NULL, 0,
                                "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: " TP_SIZE_T_FMT "\r\n\r\nok",
                                content_type, (size_t)2);
    ok("format expected long content type response", expected_len > 0);
    if (expected_len > 0)
    {
        char *expected = (char *)malloc((size_t)expected_len + 1);
        ok("allocate expected long content type response", expected != NULL);
        if (expected != NULL)
        {
            snprintf(expected, (size_t)expected_len + 1,
                     "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: " TP_SIZE_T_FMT "\r\n\r\nok",
                     content_type, (size_t)2);
            ok("long content type response length", actual_len == (size_t)expected_len);
            ok("long content type response bytes",
               actual_len == (size_t)expected_len && memcmp(actual, expected, actual_len) == 0);
            free(expected);
        }
    }

    teapot_response_free(&resp);
    close_pair(sockets);
}

static void test_content_type_rejects_response_splitting(void)
{
    int sockets[2] = {-1, -1};
    ok("socketpair for response splitting", socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    if (sockets[0] < 0 || sockets[1] < 0)
    {
        return;
    }

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = "text/plain\r\nX-Injected: yes";
    teapot_response_write(&resp, "ok", 2);

    ok("reject CRLF in content type", teapot_send_response((stb_teapot_socket_t)sockets[0], &resp) == -1);

    teapot_response_free(&resp);
    close_pair(sockets);
}

int main(void)
{
    test_long_content_type_is_sent_without_stack_overread();
    test_content_type_rejects_response_splitting();

    if (failures == 0)
    {
        printf("\nALL RESPONSE TESTS PASSED\n");
        return 0;
    }

    printf("\n%d RESPONSE TEST(S) FAILED\n", failures);
    return 1;
}
#endif
