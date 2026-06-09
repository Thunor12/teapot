#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
int main(void)
{
    printf("unit_test_request is skipped on Windows\n");
    return 0;
}
#else
#include <sys/socket.h>
#include <unistd.h>

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

static teapot_response should_not_run_handler(const teapot_request *req)
{
    (void)req;
    handler_called++;

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    tp_sb_appendf(&resp.body, "handler ran\n");
    return resp;
}

static void test_lf_only_body_is_not_parsed_as_headers(void)
{
    char raw[] =
        "POST /submit HTTP/1.1\n"
        "Host: example.test\n"
        "Content-Length: 0\n"
        "\n"
        "Authorization: injected\n";

    teapot_request req = {0};
    int rc = parse_request(raw, strlen(raw), &req);

    ok("LF-only request parses", rc == 0);
    ok("body bytes after LF delimiter are not headers",
       rc == 0 && tp_headers_get(&req.headers, "Authorization") == NULL);

    free_request(&req);
}

static void test_lowercase_content_length_keeps_initial_body(void)
{
    char raw[] =
        "POST /submit HTTP/1.1\r\n"
        "content-length: 5\r\n"
        "\r\n"
        "hello";

    teapot_request req = {0};
    int rc = parse_request(raw, strlen(raw), &req);

    ok("lowercase Content-Length parses", rc == 0);
    ok("initial body bytes are preserved",
       rc == 0 && req.body_length == 5 && memcmp(req.body.items, "hello", 5) == 0);

    free_request(&req);
}

static void test_conflicting_content_length_rejected(void)
{
    char raw[] =
        "POST /submit HTTP/1.1\r\n"
        "Content-Length: 5\r\n"
        "Content-Length: 6\r\n"
        "\r\n"
        "hello";

    teapot_request req = {0};
    int rc = parse_request(raw, strlen(raw), &req);

    ok("conflicting Content-Length is rejected", rc < 0);
}

static void test_incomplete_body_rejected_before_handler(void)
{
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0)
    {
        ok("socketpair created", 0);
        return;
    }

    handler_called = 0;
    teapot_route routes[] = {
        {TEAPOT_POST, "/upload", should_not_run_handler},
    };
    teapot_server server = {
        .port = 0,
        .routes = routes,
        .route_count = sizeof(routes) / sizeof(routes[0]),
    };

    const char request[] =
        "POST /upload HTTP/1.1\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "hello";

    ssize_t written = write(sv[0], request, sizeof(request) - 1);
    ok("partial request written", written == (ssize_t)(sizeof(request) - 1));
    shutdown(sv[0], SHUT_WR);

    int rc = teapot_handle_client_connection(&server, sv[1]);

    char response[512];
    ssize_t n = read(sv[0], response, sizeof(response) - 1);
    if (n < 0)
    {
        n = 0;
    }
    response[n] = '\0';

    ok("incomplete body returns failure", rc < 0);
    ok("handler was not called", handler_called == 0);
    ok("incomplete body receives 400",
       strstr(response, "HTTP/1.1 400 Bad Request") != NULL);

    close(sv[0]);
}

int main(void)
{
    printf("Running request parsing/handling unit tests...\n\n");

    test_lf_only_body_is_not_parsed_as_headers();
    test_lowercase_content_length_keeps_initial_body();
    test_conflicting_content_length_rejected();
    test_incomplete_body_rejected_before_handler();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
#endif
