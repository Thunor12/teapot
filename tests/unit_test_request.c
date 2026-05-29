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
    char raw[] = "POST /echo HTTP/1.1\r\n"
                 "host: example.test\r\n"
                 "content-length: 5\r\n"
                 "\r\n"
                 "hello";
    teapot_request req = {0};

    ok("parse lowercase content-length request", parse_request(raw, strlen(raw), &req) == 0);
    ok("buffered lowercase content-length body length", req.body_length == 5);
    ok("buffered lowercase content-length body bytes",
       req.body.items != NULL && memcmp(req.body.items, "hello", 5) == 0);

    free_request(&req);
}

static void test_body_content_length_text_is_not_a_header(void)
{
    char raw[] = "POST /echo HTTP/1.1\r\n"
                 "host: example.test\r\n"
                 "\r\n"
                 "Content-Length: 999";
    teapot_request req = {0};

    ok("parse request without content-length header", parse_request(raw, strlen(raw), &req) == 0);
    ok("body text does not create content-length", req.body_length == 0);

    free_request(&req);
}

int main(void)
{
    printf("Running request parsing unit tests...\n\n");

    test_lowercase_content_length_keeps_buffered_body();
    test_body_content_length_text_is_not_a_header();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
