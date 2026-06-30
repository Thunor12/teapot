#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
int main(void)
{
    puts("unit_test_response is skipped on Windows");
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

static int buffer_contains(const char *haystack, size_t haystack_len, const char *needle, size_t needle_len)
{
    if (needle_len == 0)
    {
        return 1;
    }

    if (haystack_len < needle_len)
    {
        return 0;
    }

    for (size_t i = 0; i <= haystack_len - needle_len; ++i)
    {
        if (memcmp(haystack + i, needle, needle_len) == 0)
        {
            return 1;
        }
    }

    return 0;
}

static size_t read_all(int fd, char *buf, size_t cap)
{
    size_t total = 0;
    while (total < cap)
    {
        ssize_t n = recv(fd, buf + total, cap - total, 0);
        if (n <= 0)
        {
            break;
        }
        total += (size_t)n;
    }
    return total;
}

static void test_long_content_type_is_not_truncated_or_overread(void)
{
    int fds[2];
    ok("socketpair for long content type", socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    size_t long_ct_len = 600;
    char *long_ct = (char *)malloc(long_ct_len + 1);
    ok("allocate long content type", long_ct != NULL);
    if (long_ct == NULL)
    {
        close(fds[0]);
        close(fds[1]);
        return;
    }

    const char *prefix = "application/";
    size_t prefix_len = strlen(prefix);
    memcpy(long_ct, prefix, prefix_len);
    memset(long_ct + prefix_len, 'x', long_ct_len - prefix_len);
    long_ct[long_ct_len] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = long_ct;
    teapot_response_write(&resp, "ok", 2);

    ok("send long content type response", teapot_send_response(fds[0], &resp) == 0);
    close(fds[0]);

    char buf[2048];
    size_t n = read_all(fds[1], buf, sizeof(buf));
    ok("response contains full long content type", buffer_contains(buf, n, long_ct, long_ct_len));
    ok("response has correct content length", buffer_contains(buf, n, "Content-Length: 2", strlen("Content-Length: 2")));
    ok("response body sent", n >= 2 && memcmp(buf + n - 2, "ok", 2) == 0);

    close(fds[1]);
    teapot_response_free(&resp);
    free(long_ct);
}

static void test_content_type_rejects_crlf_injection(void)
{
    int fds[2];
    ok("socketpair for crlf rejection", socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = "text/plain\r\nX-Injected: yes";
    teapot_response_write(&resp, "ok", 2);

    ok("reject injected content type", teapot_send_response(fds[0], &resp) == -1);

    close(fds[0]);
    close(fds[1]);
    teapot_response_free(&resp);
}

int main(void)
{
    test_long_content_type_is_not_truncated_or_overread();
    test_content_type_rejects_crlf_injection();

    if (failures == 0)
    {
        puts("ALL RESPONSE TESTS PASSED");
        return 0;
    }

    printf("%d RESPONSE TEST(S) FAILED\n", failures);
    return 1;
}
#endif
