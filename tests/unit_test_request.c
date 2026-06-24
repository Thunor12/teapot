#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

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

static size_t read_socket(stb_teapot_socket_t s, char *buf, size_t cap)
{
    size_t total = 0;
    while (total + 1 < cap)
    {
        ssize_t n = read(s, buf + total, cap - total - 1);
        if (n <= 0)
        {
            break;
        }
        total += (size_t)n;
    }
    buf[total] = '\0';
    return total;
}

static teapot_response echo_handler(const teapot_request *req)
{
    ++handler_calls;

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    tp_sb_appendf(&resp.body, TP_SIZE_T_FMT ":", req->body_length);
    teapot_response_write(&resp, req->body.items, req->body_length);
    return resp;
}

static teapot_server test_server(void)
{
    static teapot_route routes[] = {
        {TEAPOT_POST, "/echo", echo_handler},
    };

    teapot_server server = {
        .port = 0,
        .routes = routes,
        .route_count = sizeof(routes) / sizeof(routes[0]),
    };
    return server;
}

static size_t serve_request(const char *request, char *response, size_t response_cap)
{
    stb_teapot_socket_t fds[2];
    ok("socketpair", socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    size_t request_len = strlen(request);
    ssize_t written = write(fds[0], request, request_len);
    ok("write request", written == (ssize_t)request_len);
    shutdown(fds[0], SHUT_WR);

    teapot_server server = test_server();
    (void)teapot_handle_client_connection(&server, fds[1]);

    size_t got = read_socket(fds[0], response, response_cap);
    close(fds[0]);
    return got;
}

static void test_lowercase_content_length_keeps_buffered_body(void)
{
    handler_calls = 0;
    char response[1024];
    size_t got = serve_request(
        "POST /echo HTTP/1.1\r\n"
        "host: example.test\r\n"
        "content-length: 5\r\n"
        "content-type: text/plain\r\n"
        "\r\n"
        "hello",
        response, sizeof(response));

    ok("read lowercase content-length response", got > 0);
    ok("handler called once", handler_calls == 1);
    ok("body delivered exactly once", strstr(response, "\r\n\r\n5:hello") != NULL);
}

static void test_incomplete_body_is_rejected_before_handler(void)
{
    handler_calls = 0;
    char response[1024];
    size_t got = serve_request(
        "POST /echo HTTP/1.1\r\n"
        "Host: example.test\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "he",
        response, sizeof(response));

    ok("read incomplete body response", got > 0);
    ok("handler not called for incomplete body", handler_calls == 0);
    ok("incomplete body returns 400", strstr(response, "HTTP/1.1 400 Bad Request\r\n") != NULL);
}

static void test_invalid_content_length_is_rejected(void)
{
    handler_calls = 0;
    char response[1024];
    size_t got = serve_request(
        "POST /echo HTTP/1.1\r\n"
        "Host: example.test\r\n"
        "Content-Length: 5x\r\n"
        "\r\n"
        "hello",
        response, sizeof(response));

    ok("read invalid content-length response", got > 0);
    ok("handler not called for invalid length", handler_calls == 0);
    ok("invalid content-length returns 400", strstr(response, "HTTP/1.1 400 Bad Request\r\n") != NULL);
}

int main(void)
{
    test_lowercase_content_length_keeps_buffered_body();
    test_incomplete_body_is_rejected_before_handler();
    test_invalid_content_length_is_rejected();

    if (failures == 0)
    {
        printf("ALL REQUEST TESTS PASSED\n");
        return 0;
    }

    printf("%d REQUEST TEST(S) FAILED\n", failures);
    return 1;
}
