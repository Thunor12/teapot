#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <unistd.h>
#endif

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

static void test_content_length_uses_parsed_header(void)
{
    char raw[] =
        "POST /echo HTTP/1.1\r\n"
        "X-Pad: prefix Content-Length: 5000\r\n"
        "Content-Length: 10\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "0123456789GET /next HTTP/1.1\r\n\r\n";

    teapot_request req = {0};
    ok("parse request with fake header value", parse_request(raw, strlen(raw), &req) == 0);
    ok("body length follows real Content-Length", req.body_length == (size_t)10);
    ok("body excludes pipelined bytes", req.body.items != NULL && memcmp(req.body.items, "0123456789", (size_t)10) == 0 && req.body.items[10] == '\0');
    free_request(&req);
}

static void test_long_content_type_response_header(void)
{
#ifdef _WIN32
    printf("[SKIP] long content type socket test on Windows\n");
#else
    int sockets[2] = {-1, -1};
    ok("create socket pair", socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
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
    teapot_response_write(&resp, "ok", (size_t)2);

    ok("send response with long content type", teapot_send_response(sockets[0], &resp) == 0);

    char received[1024];
    ssize_t n = read(sockets[1], received, sizeof(received) - 1);
    ok("read response bytes", n > 0);
    if (n > 0)
    {
        received[(size_t)n] = '\0';
        ok("response contains complete content type", strstr(received, content_type) != NULL);
        ok("response body preserved", strstr(received, "\r\n\r\nok") != NULL);
    }

    teapot_response_free(&resp);
    close(sockets[0]);
    close(sockets[1]);
#endif
}

int main(void)
{
    printf("Running request/response unit tests...\n\n");

    test_content_length_uses_parsed_header();
    test_long_content_type_response_header();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
