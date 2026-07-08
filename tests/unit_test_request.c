#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

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

static size_t read_all(int fd, char *buf, size_t cap)
{
    size_t total = 0;
    while (total + 1 < cap)
    {
        ssize_t n = read(fd, buf + total, cap - total - 1);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }
        if (n == 0)
        {
            break;
        }
        total += (size_t)n;
    }
    buf[total] = '\0';
    return total;
}

static teapot_response counting_handler(const teapot_request *req)
{
    (void)req;
    ++handler_calls;

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    teapot_response_write(&resp, "handled", 7);
    return resp;
}

static void test_method_token_must_match_exactly(void)
{
    char request[] = "POSTMAN /echo HTTP/1.1\r\n\r\n";
    teapot_request req = {0};

    ok("prefixed method is rejected", parse_request(request, strlen(request), &req) < 0);
    free_request(&req);
}

static void test_case_insensitive_content_length_keeps_initial_body(void)
{
    char request[] = "POST /echo HTTP/1.1\r\ncontent-length: 5\r\n\r\nhello";
    teapot_request req = {0};

    ok("lowercase content-length parses", parse_request(request, strlen(request), &req) == 0);
    ok("initial body length preserved", req.body_length == 5);
    ok("initial body bytes preserved", req.body.items != NULL && memcmp(req.body.items, "hello", 5) == 0);

    free_request(&req);
}

static void test_incomplete_body_does_not_reach_handler(void)
{
    int sv[2];
    ok("socketpair for incomplete body", socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

    const char request[] = "POST /echo HTTP/1.1\r\nContent-Length: 5\r\n\r\nhi";
    size_t request_len = strlen(request);
    ssize_t sent = write(sv[0], request, request_len);
    ok("write partial request", sent >= 0 && (size_t)sent == request_len);
    (void)shutdown(sv[0], SHUT_WR);

    teapot_route routes[] = {
        {TEAPOT_POST, "/echo", counting_handler},
    };
    teapot_server server = {
        .port = 0,
        .routes = routes,
        .route_count = sizeof(routes) / sizeof(routes[0]),
    };

    handler_calls = 0;
    ok("incomplete body returns an error", teapot_handle_client_connection(&server, sv[1]) < 0);
    ok("handler was not called", handler_calls == 0);

    char response[512];
    size_t response_len = read_all(sv[0], response, sizeof(response));
    ok("received bad request response", response_len > 0 && strstr(response, "400 Bad Request") != NULL);

    close(sv[0]);
}

int main(void)
{
    test_method_token_must_match_exactly();
    test_case_insensitive_content_length_keeps_initial_body();
    test_incomplete_body_does_not_reach_handler();

    if (failures == 0)
    {
        printf("ALL REQUEST TESTS PASSED\n");
        return 0;
    }

    printf("%d REQUEST TEST(S) FAILED\n", failures);
    return 1;
}
