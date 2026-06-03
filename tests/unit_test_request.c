#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
int main(void)
{
    puts("unit_test_request skipped on Windows");
    return 0;
}
#else
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

static int write_all_fd(int fd, const char *buf, size_t len)
{
    size_t written = 0;
    while (written < len)
    {
        ssize_t n = write(fd, buf + written, len - written);
        if (n <= 0)
        {
            return -1;
        }
        written += (size_t)n;
    }
    return 0;
}

static teapot_response counting_handler(const teapot_request *req)
{
    ++handler_calls;

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    tp_sb_appendf(&resp.body, "body:%zu:%s", req->body_length, req->body.items ? req->body.items : "");
    return resp;
}

static void test_lowercase_content_length_preserves_buffered_body(void)
{
    char raw[] =
        "POST /echo HTTP/1.1\r\n"
        "host: example.test\r\n"
        "content-length: 5\r\n"
        "content-type: text/plain\r\n"
        "\r\n"
        "hello";
    teapot_request req = {0};

    ok("parse lower-case content-length request", parse_request(raw, strlen(raw), &req) == 0);
    ok("buffered body length preserved", req.body_length == 5);
    ok("buffered body bytes preserved", req.body.items != NULL && memcmp(req.body.items, "hello", 5) == 0 && req.body.items[5] == '\0');

    free_request(&req);
}

static void test_incomplete_body_rejected_before_handler(void)
{
    int fds[2] = {-1, -1};
    ok("socketpair created", socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    if (fds[0] < 0 || fds[1] < 0)
    {
        return;
    }

    const char request[] =
        "POST /echo HTTP/1.1\r\n"
        "Host: example.test\r\n"
        "Content-Length: 5\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "he";
    ok("short request written", write_all_fd(fds[0], request, strlen(request)) == 0);
    ok("client write side closed", shutdown(fds[0], SHUT_WR) == 0);

    teapot_route routes[] = {
        {TEAPOT_POST, "/echo", counting_handler},
    };
    teapot_server server = {
        .port = 0,
        .routes = routes,
        .route_count = sizeof(routes) / sizeof(routes[0]),
    };

    handler_calls = 0;
    (void)teapot_handle_client_connection(&server, (stb_teapot_socket_t)fds[1]);
    ok("handler not called for incomplete body", handler_calls == 0);

    char response[256];
    ssize_t n = read(fds[0], response, sizeof(response) - 1);
    ok("received rejection response", n > 0);
    if (n > 0)
    {
        response[(size_t)n] = '\0';
        ok("incomplete body rejected with 400", strstr(response, "HTTP/1.1 400 Bad Request") != NULL);
    }

    close(fds[0]);
}

int main(void)
{
    test_lowercase_content_length_preserves_buffered_body();
    test_incomplete_body_rejected_before_handler();

    if (failures == 0)
    {
        puts("ALL TESTS PASSED");
        return 0;
    }

    printf("%d TEST(S) FAILED\n", failures);
    return 1;
}
#endif
