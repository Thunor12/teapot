#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
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

static void test_lowercase_content_length_keeps_buffered_body(void)
{
    char raw[] =
        "POST /echo HTTP/1.1\r\n"
        "host: example.test\r\n"
        "content-type: text/plain\r\n"
        "content-length: 5\r\n"
        "\r\n"
        "hello";
    teapot_request req = {0};

    ok("parse lowercase content-length request", parse_request(raw, strlen(raw), &req) == 0);
    ok("buffered body length retained", req.body_length == 5);
    ok("buffered body bytes retained", req.body.items != NULL && memcmp(req.body.items, "hello", 5) == 0);

    free_request(&req);
}

static void test_body_text_does_not_fake_content_length_header(void)
{
    char raw[] =
        "POST /echo HTTP/1.1\r\n"
        "host: example.test\r\n"
        "\r\n"
        "Content-Length: 999\r\nabc";
    teapot_request req = {0};

    ok("parse request without content-length header", parse_request(raw, strlen(raw), &req) == 0);
    ok("body text content-length is ignored", req.body_length == 0);

    free_request(&req);
}

int main(void)
{
    test_lowercase_content_length_keeps_buffered_body();
    test_body_text_does_not_fake_content_length_header();

    if (failures == 0)
    {
        printf("ALL REQUEST TESTS PASSED\n");
        return 0;
    }

    printf("%d REQUEST TEST(S) FAILED\n", failures);
    return 1;
}
