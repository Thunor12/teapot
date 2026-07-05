#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void ok(const char *name, int cond)
{
    if (cond)
        printf("[PASS] %s\n", name);
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

static void test_case_insensitive_content_length_preserves_buffered_body(void)
{
    char raw[] = "POST /submit HTTP/1.1\r\nHost: example\r\ncontent-length: 5\r\n\r\nhello";
    teapot_request req = {0};

    int rc = parse_request(raw, strlen(raw), &req);
    ok("lowercase content-length parses", rc == 0);
    ok("buffered body length preserved", req.body_length == 5);
    ok("buffered body bytes preserved", req.body.items != NULL && memcmp(req.body.items, "hello", 5) == 0);

    free_request(&req);
}

static void test_invalid_methods_are_rejected(void)
{
    char deleteable[] = "DELETEABLE /resource HTTP/1.1\r\n\r\n";
    char putty[] = "PUTTY /resource HTTP/1.1\r\n\r\n";
    teapot_request req = {0};

    ok("DELETE prefix is not DELETE", parse_request(deleteable, strlen(deleteable), &req) < 0);
    free_request(&req);

    req = (teapot_request){0};
    ok("PUT prefix is not PUT", parse_request(putty, strlen(putty), &req) < 0);
    free_request(&req);
}

static void test_invalid_and_oversized_content_length_rejected(void)
{
    char invalid[] = "POST /submit HTTP/1.1\r\nContent-Length: five\r\n\r\n";
    char oversized[128];
    teapot_request req = {0};

    ok("invalid content-length rejected", parse_request(invalid, strlen(invalid), &req) < 0);
    free_request(&req);

    snprintf(oversized, sizeof(oversized), "POST /submit HTTP/1.1\r\nContent-Length: %zu\r\n\r\n", TEAPOT_MAX_BODY_SIZE + 1);
    req = (teapot_request){0};
    ok("oversized content-length rejected", parse_request(oversized, strlen(oversized), &req) == -2);
    free_request(&req);
}

static void test_wildcard_route_matches_nested_path(void)
{
    char raw[] = "GET /api/widgets HTTP/1.1\r\n\r\n";
    teapot_request req = {0};
    teapot_route routes[] = {
        {TEAPOT_GET, "/api/*", dummy_handler},
    };
    teapot_server server = {0, routes, sizeof(routes) / sizeof(routes[0])};

    int rc = parse_request(raw, strlen(raw), &req);
    ok("wildcard request parses", rc == 0);
    ok("wildcard route matches nested path", teapot_find_handler(&server, &req) == dummy_handler);

    free_request(&req);
}

int main(void)
{
    printf("Running request unit tests...\n\n");

    test_case_insensitive_content_length_preserves_buffered_body();
    test_invalid_methods_are_rejected();
    test_invalid_and_oversized_content_length_rejected();
    test_wildcard_route_matches_nested_path();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
