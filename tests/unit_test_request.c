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

static void test_content_length_comes_from_header_name(void)
{
    char request[] =
        "POST /echo HTTP/1.1\r\n"
        "X-Note: Content-Length: 0\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "hello";
    teapot_request req = {0};

    ok("parse request succeeds", parse_request(request, sizeof(request) - 1, &req) == 0);
    ok("body length uses real Content-Length header", req.body_length == 5);
    ok("body bytes are preserved", req.body.items != NULL && memcmp(req.body.items, "hello", 5) == 0);

    free_request(&req);
}

int main(void)
{
    test_content_length_comes_from_header_name();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
