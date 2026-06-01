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
    char request[] = "POST /echo HTTP/1.1\r\n"
                     "content-length: 5\r\n"
                     "Content-Type: text/plain\r\n"
                     "\r\n"
                     "hello";
    teapot_request req = {0};

    ok("parse lowercase content-length", parse_request(request, strlen(request), &req) == 0);
    ok("body length from structured header", req.body_length == 5);
    ok("body bytes retained", req.body.items != NULL && memcmp(req.body.items, "hello", 5) == 0);

    free_request(&req);
}

static void test_body_text_is_not_parsed_as_header(void)
{
    char request[] = "POST /echo HTTP/1.1\r\n"
                     "Content-Type: text/plain\r\n"
                     "\r\n"
                     "Content-Length: 5";
    teapot_request req = {0};

    ok("parse body containing header-like text", parse_request(request, strlen(request), &req) == 0);
    ok("no header content-length means no body consumed", req.body_length == 0);

    free_request(&req);
}

int main(void)
{
    test_lowercase_content_length_keeps_initial_body();
    test_body_text_is_not_parsed_as_header();
    return failures == 0 ? 0 : 1;
}
