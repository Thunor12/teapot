#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
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

static ssize_t send_response_and_read(teapot_response *resp, char *out, size_t out_size)
{
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0)
    {
        return -1;
    }

    int rc = teapot_send_response(sv[1], resp);
    teapot_close(sv[1]);
    if (rc < 0)
    {
        close(sv[0]);
        return -1;
    }

    ssize_t total = 0;
    while ((size_t)total + 1 < out_size)
    {
        ssize_t n = read(sv[0], out + total, out_size - (size_t)total - 1);
        if (n <= 0)
        {
            break;
        }
        total += n;
    }
    out[total] = '\0';
    close(sv[0]);
    return total;
}

static void test_long_content_type_does_not_truncate_header(void)
{
    char long_content_type[420];
    memset(long_content_type, 'a', sizeof(long_content_type) - 1);
    long_content_type[sizeof(long_content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = long_content_type;
    teapot_response_write(&resp, "OK", 2);

    char wire[1024];
    ssize_t n = send_response_and_read(&resp, wire, sizeof(wire));

    ok("long content type response sent", n > 0);
    ok("long content type keeps complete header terminator",
       n > 0 && strstr(wire, "\r\n\r\nOK") != NULL);
    ok("long content type keeps correct content length",
       n > 0 && strstr(wire, "Content-Length: 2\r\n\r\nOK") != NULL);

    teapot_response_free(&resp);
}

static void test_content_type_crlf_is_not_injected(void)
{
    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = "text/plain\r\nX-Injected: yes";
    teapot_response_write(&resp, "OK", 2);

    char wire[512];
    ssize_t n = send_response_and_read(&resp, wire, sizeof(wire));

    ok("CRLF content type response sent", n > 0);
    ok("CRLF content type does not inject a header",
       n > 0 && strstr(wire, "X-Injected") == NULL);
    ok("CRLF content type falls back to safe default",
       n > 0 && strstr(wire, "Content-Type: text/plain\r\n") != NULL);

    teapot_response_free(&resp);
}

int main(void)
{
    printf("Running response unit tests...\n\n");

    test_long_content_type_does_not_truncate_header();
    test_content_type_crlf_is_not_injected();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
#endif
