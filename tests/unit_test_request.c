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

static teapot_response dummy_handler(const teapot_request *req)
{
    (void)req;
    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    return resp;
}

static void test_lowercase_content_length_preserves_buffered_body(void)
{
    char raw[] =
        "POST /echo HTTP/1.1\r\n"
        "content-length: 5\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "hello";
    teapot_request req = {0};

    ok("parse lowercase content-length request", parse_request(raw, strlen(raw), &req) == 0);
    ok("lowercase content-length body length", req.body_length == 5);
    ok("lowercase content-length body bytes",
       req.body.items != NULL && memcmp(req.body.items, "hello", 5) == 0 && req.body.items[5] == '\0');

    free_request(&req);
}

static void test_invalid_content_length_is_rejected(void)
{
    char raw[] =
        "POST /echo HTTP/1.1\r\n"
        "Content-Length: five\r\n"
        "\r\n"
        "hello";
    teapot_request req = {0};

    ok("reject invalid content-length", parse_request(raw, strlen(raw), &req) < 0);
    free_request(&req);
}

static void test_method_tokens_are_exact(void)
{
    char raw[] =
        "GETX /hello HTTP/1.1\r\n"
        "\r\n";
    teapot_request req = {0};

    ok("reject method token with valid prefix", parse_request(raw, strlen(raw), &req) < 0);
    free_request(&req);
}

static void test_wildcard_route_matches_subpath(void)
{
    teapot_route routes[] = {
        {TEAPOT_GET, "/api/*", dummy_handler},
    };
    teapot_server server = {
        .port = 0,
        .routes = routes,
        .route_count = sizeof(routes) / sizeof(routes[0]),
    };
    teapot_request req = {0};
    req.method = TEAPOT_GET;
    tp_sb_append_cstr(&req.path, "/api/users");
    tp_sb_append_null(&req.path);

    ok("wildcard route matches subpath", teapot_find_handler(&server, &req) == dummy_handler);

    free_request(&req);
}

int main(void)
{
    test_lowercase_content_length_preserves_buffered_body();
    test_invalid_content_length_is_rejected();
    test_method_tokens_are_exact();
    test_wildcard_route_matches_subpath();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
