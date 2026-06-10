#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <unistd.h>
#endif

static int failures = 0;
static int handler_called = 0;

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

static void test_structured_content_length_wins(void)
{
    char raw[] =
        "POST /echo HTTP/1.1\r\n"
        "X-Fake: Content-Length: 1\r\n"
        "content-length: 5\r\n"
        "\r\n"
        "helloEXTRA";

    teapot_request req = {0};
    int rc = parse_request(raw, strlen(raw), &req);
    ok("parse request with lowercase content-length", rc == 0);
    ok("body length uses structured Content-Length", req.body_length == 5);
    ok("body excludes pipelined bytes", req.body.items != NULL && memcmp(req.body.items, "hello", 5) == 0);
    free_request(&req);
}

static void test_method_must_match_exactly(void)
{
    char raw[] = "GETS /hello HTTP/1.1\r\n\r\n";
    teapot_request req = {0};
    int rc = parse_request(raw, strlen(raw), &req);
    ok("method token is exact", rc < 0);
    free_request(&req);
}

#ifndef _WIN32
static teapot_response incomplete_body_handler(const teapot_request *req)
{
    (void)req;
    ++handler_called;

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    teapot_response_write(&resp, "unexpected", strlen("unexpected"));
    return resp;
}

static void test_incomplete_body_is_rejected_before_handler(void)
{
    int sv[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
    {
        ok("socketpair created", 0);
        return;
    }

    const char request[] =
        "POST /echo HTTP/1.1\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "he";
    ssize_t sent = send(sv[0], request, strlen(request), 0);
    ok("partial request sent", sent > 0);
    shutdown(sv[0], SHUT_WR);

    teapot_route routes[] = {
        {TEAPOT_POST, "/echo", incomplete_body_handler},
    };
    teapot_server server = {
        .port = 0,
        .routes = routes,
        .route_count = sizeof(routes) / sizeof(routes[0]),
    };

    handler_called = 0;
    int rc = teapot_handle_client_connection(&server, sv[1]);
    ok("incomplete body rejected", rc < 0);
    ok("handler not called for incomplete body", handler_called == 0);

    close(sv[0]);
}
#endif

int main(void)
{
    printf("Running request unit tests...\n\n");

    test_structured_content_length_wins();
    test_method_must_match_exactly();
#ifndef _WIN32
    test_incomplete_body_is_rejected_before_handler();
#endif

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
