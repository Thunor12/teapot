#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>
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

static int make_socket_pair(int sv[2])
{
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
    {
        perror("socketpair");
        return 0;
    }
    return 1;
}

static int write_all(int fd, const char *buf, size_t len)
{
    size_t written = 0;
    while (written < len)
    {
        ssize_t n = write(fd, buf + written, len - written);
        if (n <= 0)
        {
            return 0;
        }
        written += (size_t)n;
    }
    return 1;
}

static size_t read_response(int fd, char *buf, size_t cap)
{
    size_t total = 0;
    while (total + 1 < cap)
    {
        ssize_t n = read(fd, buf + total, cap - total - 1);
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
    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    ++handler_called;

    if (req->body_length == 5 && req->body.items != NULL && memcmp(req->body.items, "hello", 5) == 0)
    {
        teapot_response_write(&resp, "ok", 2);
    }
    else
    {
        tp_sb_appendf(&resp.body, "bad body length " TP_SIZE_T_FMT, req->body_length);
    }

    return resp;
}

static teapot_server make_server(void)
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

static void test_lowercase_content_length_buffered_body(void)
{
    int sv[2] = {-1, -1};
    if (!make_socket_pair(sv))
    {
        ++failures;
        return;
    }

    const char request[] =
        "POST /echo HTTP/1.1\r\n"
        "host: example.test\r\n"
        "content-length: 5\r\n"
        "\r\n"
        "hello";
    ok("write request", write_all(sv[0], request, strlen(request)));
    shutdown(sv[0], SHUT_WR);

    handler_called = 0;
    teapot_server server = make_server();
    ok("handle lowercase content-length", teapot_handle_client_connection(&server, sv[1]) == 0);
    ok("handler called once", handler_called == 1);

    char response[512];
    read_response(sv[0], response, sizeof(response));
    ok("handler saw complete buffered body", strstr(response, "\r\n\r\nok") != NULL);

    close(sv[0]);
}

static void test_incomplete_content_length_rejected(void)
{
    int sv[2] = {-1, -1};
    if (!make_socket_pair(sv))
    {
        ++failures;
        return;
    }

    const char request[] =
        "POST /echo HTTP/1.1\r\n"
        "Host: example.test\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "short";
    ok("write incomplete request", write_all(sv[0], request, strlen(request)));
    shutdown(sv[0], SHUT_WR);

    handler_called = 0;
    teapot_server server = make_server();
    ok("incomplete body is rejected", teapot_handle_client_connection(&server, sv[1]) == -1);
    ok("handler not called for truncated body", handler_called == 0);

    close(sv[0]);
}

static void test_partial_headers_are_rejected_by_parser(void)
{
    char request[] =
        "POST /echo HTTP/1.1\r\n"
        "Content-Length: 5";
    teapot_request req = {0};
    ok("partial headers rejected", parse_request(request, strlen(request), &req) == -1);
    free_request(&req);
}

int main(void)
{
    test_lowercase_content_length_buffered_body();
    test_incomplete_content_length_rejected();
    test_partial_headers_are_rejected_by_parser();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
