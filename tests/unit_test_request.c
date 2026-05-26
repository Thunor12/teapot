#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void ok(const char *name, int condition)
{
    if (condition)
    {
        printf("[PASS] %s\n", name);
    }
    else
    {
        printf("[FAIL] %s\n", name);
        failures++;
    }
}

static void test_lowercase_content_length_preserves_initial_body(void)
{
    char raw[] = "POST /echo HTTP/1.1\r\ncontent-length: 5\r\n\r\nhello";
    teapot_request req = {0};

    ok("lowercase content-length parses", parse_request(raw, strlen(raw), &req) == 0);
    ok("initial body bytes retained", req.body_length == 5 && req.body.items != NULL && memcmp(req.body.items, "hello", 5) == 0);

    free_request(&req);
}

static void test_body_text_does_not_define_content_length(void)
{
    char raw[] = "POST /echo HTTP/1.1\r\nHost: example\r\n\r\nContent-Length: 999\r\nabc";
    teapot_request req = {0};

    ok("request without content-length parses", parse_request(raw, strlen(raw), &req) == 0);
    ok("content-length in body is ignored", req.body_length == 0);

    free_request(&req);
}

int main(void)
{
    printf("Running request unit tests...\n\n");

    test_lowercase_content_length_preserves_initial_body();
    test_body_text_does_not_define_content_length();

    if (failures)
    {
        printf("\n%d request test(s) failed\n", failures);
        return 1;
    }

    printf("\nAll request tests passed.\n");
    return 0;
}
