#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <sys/socket.h>
#endif

static int failures = 0;
static int handler_calls = 0;

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

static teapot_response marker_handler(const teapot_request *req)
{
    (void)req;
    ++handler_calls;

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    tp_sb_appendf(&resp.body, "handled\n");
    return resp;
}

static void test_wildcard_route_matches_subpath(void)
{
    teapot_route routes[] = {
        {TEAPOT_GET, "/api/*", marker_handler},
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

    ok("wildcard /api/* matches /api/users", teapot_find_handler(&server, &req) == marker_handler);

    free_request(&req);
}

static void test_parse_lowercase_content_length_body(void)
{
    char raw[] = "POST /echo HTTP/1.1\r\nhost: example\r\ncontent-length: 5\r\n\r\nhello";
    teapot_request req = {0};

    ok("parse lowercase content-length succeeds", parse_request(raw, sizeof(raw) - 1u, &req) == 0);
    ok("lowercase content-length preserves initial body length", req.body_length == 5u);
    ok("lowercase content-length preserves initial body bytes", req.body.items && strcmp(req.body.items, "hello") == 0);

    free_request(&req);
}

#ifndef _WIN32
static void test_oversize_content_length_rejected_before_handler(void)
{
    int fds[2];
    char request[256];
    char response[512];
    int n;
    int pair_ok;

    handler_calls = 0;
    pair_ok = socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0;
    ok("socketpair for oversized request", pair_ok);
    if (!pair_ok)
    {
        return;
    }

    n = snprintf(request, sizeof(request),
                 "POST /upload HTTP/1.1\r\nContent-Length: %zu\r\n\r\nabc",
                 (size_t)TEAPOT_MAX_REQUEST_BODY_BYTES + 1u);
    ok("format oversized request", n > 0 && (size_t)n < sizeof(request));

    teapot_route routes[] = {
        {TEAPOT_POST, "/upload", marker_handler},
    };
    teapot_server server = {
        .port = 0,
        .routes = routes,
        .route_count = sizeof(routes) / sizeof(routes[0]),
    };

    ok("send oversized request", send(fds[0], request, (size_t)n, 0) == (ssize_t)n);
    ok("oversized request returns error", teapot_handle_client_connection(&server, fds[1]) == -1);

    n = (int)recv(fds[0], response, sizeof(response) - 1u, 0);
    ok("read 413 response", n > 0);
    if (n > 0)
    {
        response[n] = '\0';
        ok("oversized request status is 413", strstr(response, "HTTP/1.1 413 Payload Too Large") != NULL);
    }
    ok("oversized request does not call handler", handler_calls == 0);

    teapot_close((stb_teapot_socket_t)fds[0]);
}
#endif

int main(void)
{
    printf("Running request unit tests...\n\n");

    test_wildcard_route_matches_subpath();
    test_parse_lowercase_content_length_body();
#ifndef _WIN32
    test_oversize_content_length_rejected_before_handler();
#endif

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
