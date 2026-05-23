#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
int main(void)
{
    puts("request/response socket tests are skipped on Windows");
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

static void test_content_length_comes_from_header_name(void)
{
    char req_buf[] =
        "POST /echo HTTP/1.1\r\n"
        "X-Ignore: Content-Length: 40\r\n"
        "Content-Length: 1\r\n"
        "\r\n"
        "AEXTRA";
    teapot_request req = {0};

    ok("parse request with fake content length", parse_request(req_buf, strlen(req_buf), &req) == 0);
    ok("real content length controls body size", req.body_length == 1);
    ok("body contains only declared byte", req.body.items && strcmp(req.body.items, "A") == 0);

    free_request(&req);
}

static void test_long_content_type_response_header(void)
{
    int sv[2] = {-1, -1};
    ok("socketpair created", socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    if (sv[0] < 0 || sv[1] < 0)
    {
        return;
    }

    char content_type[600];
    memset(content_type, 'a', sizeof(content_type) - 1);
    content_type[sizeof(content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "OK", 2);

    ok("send response with long content type", teapot_send_response((stb_teapot_socket_t)sv[0], &resp) == 0);

    char got[1024];
    ssize_t n = read(sv[1], got, sizeof(got) - 1);
    ok("read response bytes", n > 0);
    if (n > 0)
    {
        got[(size_t)n] = '\0';
        ok("long content type is not truncated", strstr(got, content_type) != NULL);
        ok("body follows complete header", strstr(got, "\r\n\r\nOK") != NULL);
    }

    teapot_response_free(&resp);
    close(sv[0]);
    close(sv[1]);
}

int main(void)
{
    test_content_length_comes_from_header_name();
    test_long_content_type_response_header();

    if (failures == 0)
    {
        puts("ALL REQUEST/RESPONSE TESTS PASSED");
        return 0;
    }

    printf("%d TEST(S) FAILED\n", failures);
    return 1;
}
#endif
