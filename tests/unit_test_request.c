#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
int main(void)
{
    puts("unit_test_request is skipped on Windows");
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

static int buffer_contains(const char *haystack, size_t haystack_len, const char *needle)
{
    size_t needle_len = strlen(needle);
    if (needle_len == 0)
    {
        return 1;
    }

    if (haystack_len < needle_len)
    {
        return 0;
    }

    for (size_t i = 0; i <= haystack_len - needle_len; ++i)
    {
        if (memcmp(haystack + i, needle, needle_len) == 0)
        {
            return 1;
        }
    }

    return 0;
}

static teapot_response echo_handler(const teapot_request *req)
{
    ++handler_calls;

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    tp_sb_appendf(&resp.body, TP_SIZE_T_FMT ":%s", req->body_length, req->body.items ? req->body.items : "");
    return resp;
}

static size_t read_all(int fd, char *buf, size_t cap)
{
    size_t total = 0;
    while (total < cap)
    {
        ssize_t n = recv(fd, buf + total, cap - total, 0);
        if (n <= 0)
        {
            break;
        }
        total += (size_t)n;
    }
    return total;
}

static size_t round_trip(const char *request, char *response, size_t response_cap)
{
    int fds[2];
    ok("socketpair for request", socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    size_t request_len = strlen(request);
    ssize_t written = send(fds[1], request, request_len, 0);
    ok("write request", written == (ssize_t)request_len);
    shutdown(fds[1], SHUT_WR);

    teapot_route routes[] = {
        {TEAPOT_POST, "/echo", echo_handler},
    };
    teapot_server server = {
        .port = 0,
        .routes = routes,
        .route_count = sizeof(routes) / sizeof(routes[0]),
    };

    teapot_handle_client_connection(&server, fds[0]);
    size_t n = read_all(fds[1], response, response_cap);
    close(fds[1]);
    return n;
}

static void test_lowercase_content_length_preserves_buffered_body(void)
{
    handler_calls = 0;
    char response[1024];
    const char *request = "POST /echo HTTP/1.1\r\nhost: example\r\ncontent-length: 5\r\n\r\nhello";

    size_t n = round_trip(request, response, sizeof(response));

    ok("lowercase content-length reaches handler", handler_calls == 1);
    ok("lowercase content-length response ok", buffer_contains(response, n, "HTTP/1.1 200 OK"));
    ok("lowercase content-length body preserved", buffer_contains(response, n, "\r\n\r\n5:hello"));
}

static void test_body_text_does_not_create_content_length_header(void)
{
    handler_calls = 0;
    char response[1024];
    const char *request = "POST /echo HTTP/1.1\r\nHost: example\r\n\r\nContent-Length: 5\r\n\r\nhello";

    size_t n = round_trip(request, response, sizeof(response));

    ok("fake content-length reaches handler once", handler_calls == 1);
    ok("fake content-length body ignored without header", buffer_contains(response, n, "\r\n\r\n0:"));
}

static void test_incomplete_body_is_rejected_before_handler(void)
{
    handler_calls = 0;
    char response[1024];
    const char *request = "POST /echo HTTP/1.1\r\nContent-Length: 10\r\n\r\nhello";

    size_t n = round_trip(request, response, sizeof(response));

    ok("incomplete body does not reach handler", handler_calls == 0);
    ok("incomplete body gets bad request", buffer_contains(response, n, "HTTP/1.1 400 Bad Request"));
}

int main(void)
{
    test_lowercase_content_length_preserves_buffered_body();
    test_body_text_does_not_create_content_length_header();
    test_incomplete_body_is_rejected_before_handler();

    if (failures == 0)
    {
        puts("ALL REQUEST TESTS PASSED");
        return 0;
    }

    printf("%d REQUEST TEST(S) FAILED\n", failures);
    return 1;
}
#endif
