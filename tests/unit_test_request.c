#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int failures = 0;
static int handler_calls = 0;
static const char *expected_body = NULL;
static size_t expected_body_len = 0;
static int observed_body_match = 0;

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

static teapot_response ok_handler(const teapot_request *req)
{
    (void)req;
    ++handler_calls;
    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    teapot_response_write(&resp, "ok", 2);
    return resp;
}

static teapot_response echo_handler(const teapot_request *req)
{
    ++handler_calls;
    if (expected_body != NULL && req->body_length == expected_body_len && req->body.items != NULL &&
        memcmp(req->body.items, expected_body, expected_body_len) == 0)
    {
        observed_body_match = 1;
    }

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    teapot_response_write(&resp, "echo", 4);
    return resp;
}

static size_t read_response(int fd, char *buf, size_t cap)
{
    size_t total = 0;
    while (total + 1 < cap)
    {
        ssize_t n = recv(fd, buf + total, cap - total - 1, 0);
        if (n <= 0)
        {
            break;
        }
        total += (size_t)n;
    }
    buf[total] = '\0';
    return total;
}

static int drive_request(const char *request, char *response, size_t response_cap, size_t *out_response_len)
{
    int sv[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
    {
        return -100;
    }

    size_t request_len = strlen(request);
    size_t sent = 0;
    while (sent < request_len)
    {
        ssize_t n = send(sv[1], request + sent, request_len - sent, 0);
        if (n <= 0)
        {
            close(sv[0]);
            close(sv[1]);
            return -101;
        }
        sent += (size_t)n;
    }
    shutdown(sv[1], SHUT_WR);

    teapot_route routes[] = {
        {TEAPOT_GET, "/ok", ok_handler},
        {TEAPOT_POST, "/echo", echo_handler},
    };
    teapot_server server = {
        .port = 0,
        .routes = routes,
        .route_count = sizeof(routes) / sizeof(routes[0]),
    };

    int result = teapot_handle_client_connection(&server, sv[0]);
    size_t response_len = read_response(sv[1], response, response_cap);
    if (out_response_len != NULL)
    {
        *out_response_len = response_len;
    }
    close(sv[1]);
    return result;
}

static void reset_observations(void)
{
    handler_calls = 0;
    expected_body = NULL;
    expected_body_len = 0;
    observed_body_match = 0;
}

static void test_lowercase_content_length_body(void)
{
    reset_observations();
    expected_body = "hello";
    expected_body_len = 5;
    char response[512];
    size_t response_len = 0;
    int result = drive_request(
        "POST /echo HTTP/1.1\r\n"
        "content-length: 5\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "hello",
        response, sizeof(response), &response_len);

    ok("lowercase content-length request handled", result == 0);
    ok("lowercase content-length reached handler", handler_calls == 1);
    ok("lowercase content-length body preserved", observed_body_match == 1);
    ok("lowercase content-length response ok", response_len > 0 && strstr(response, "HTTP/1.1 200 OK") != NULL);
}

static void test_incomplete_body_rejected(void)
{
    reset_observations();
    char response[512];
    size_t response_len = 0;
    int result = drive_request(
        "POST /echo HTTP/1.1\r\n"
        "Content-Length: 5\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "hi",
        response, sizeof(response), &response_len);

    ok("incomplete body returns failure", result < 0);
    ok("incomplete body does not reach handler", handler_calls == 0);
    ok("incomplete body gets 400", response_len > 0 && strstr(response, "HTTP/1.1 400 Bad Request") != NULL);
}

static void test_invalid_method_rejected(void)
{
    reset_observations();
    char response[512];
    size_t response_len = 0;
    int result = drive_request("GETT /ok HTTP/1.1\r\n\r\n", response, sizeof(response), &response_len);

    ok("invalid method returns failure", result < 0);
    ok("invalid method does not reach handler", handler_calls == 0);
    ok("invalid method gets 400", response_len > 0 && strstr(response, "HTTP/1.1 400 Bad Request") != NULL);
}

static void test_incomplete_headers_rejected(void)
{
    reset_observations();
    char response[512];
    size_t response_len = 0;
    int result = drive_request("GET /ok HTTP/1.1\r\nHost: example", response, sizeof(response), &response_len);

    ok("incomplete headers return failure", result < 0);
    ok("incomplete headers do not reach handler", handler_calls == 0);
    ok("incomplete headers produce no handler response", response_len == 0);
}

int main(void)
{
    printf("Running request unit tests...\n\n");
    test_lowercase_content_length_body();
    test_incomplete_body_rejected();
    test_invalid_method_rejected();
    test_incomplete_headers_rejected();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
