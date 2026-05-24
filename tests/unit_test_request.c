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

static void test_lowercase_content_length_keeps_initial_body(void)
{
    char raw[] = "POST /echo HTTP/1.1\r\ncontent-length: 5\r\n\r\nhello";
    teapot_request req = {0};

    ok("lowercase content-length parses", parse_request(raw, strlen(raw), &req) == 0);
    ok("lowercase content-length body length", req.body_length == 5);
    ok("lowercase content-length body", req.body.items != NULL && memcmp(req.body.items, "hello", 5) == 0);

    free_request(&req);
}

static void test_body_text_does_not_define_content_length(void)
{
    char raw[] = "POST /echo HTTP/1.1\r\nHost: localhost\r\n\r\nContent-Length: 5\r\nhello";
    teapot_request req = {0};

    ok("missing content-length parses", parse_request(raw, strlen(raw), &req) == 0);
    ok("body text is not treated as header", req.body_length == 0);

    free_request(&req);
}

int main(void)
{
    test_lowercase_content_length_keeps_initial_body();
    test_body_text_does_not_define_content_length();
    return failures == 0 ? 0 : 1;
}
