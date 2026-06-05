#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#ifdef _WIN32
#include <stdio.h>

int main(void)
{
    printf("unit_test_request is skipped on Windows because it uses socketpair.\n");
    return 0;
}

#else
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

static int test_write_all(int fd, const char *buf, size_t len)
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

static teapot_response probe_handler(const teapot_request *req)
{
    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    ++handler_calls;

    if (req->body_length == 5 && req->body.items && memcmp(req->body.items, "hello", 5) == 0)
    {
        tp_sb_append_cstr(&resp.body, "body=hello\n");
    }
    else
    {
        resp.status = TEAPOT_HTTP_INTERNAL_ERROR;
        tp_sb_appendf(&resp.body, "unexpected body length %zu\n", req->body_length);
    }

    return resp;
}

static int run_request(const char *request, char *response, size_t response_size)
{
    int sockets[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
    {
        return -1;
    }

    if (test_write_all(sockets[0], request, strlen(request)) != 0)
    {
        close(sockets[0]);
        close(sockets[1]);
        return -1;
    }
    shutdown(sockets[0], SHUT_WR);

    teapot_route routes[] = {
        {TEAPOT_POST, "/echo", probe_handler},
    };
    teapot_server server = {
        .port = 0,
        .routes = routes,
        .route_count = sizeof(routes) / sizeof(routes[0]),
    };

    int handle_result = teapot_handle_client_connection(&server, (stb_teapot_socket_t)sockets[1]);
    size_t total = 0;
    while (total + 1 < response_size)
    {
        ssize_t n = read(sockets[0], response + total, response_size - total - 1);
        if (n <= 0)
        {
            break;
        }
        total += (size_t)n;
    }
    response[total] = '\0';
    close(sockets[0]);
    return handle_result;
}

static void test_lowercase_content_length_one_packet(void)
{
    handler_calls = 0;
    char response[1024];
    const char *request =
        "POST /echo HTTP/1.1\r\n"
        "host: example.test\r\n"
        "content-length: 5\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "hello";

    int result = run_request(request, response, sizeof(response));
    ok("lowercase content-length request handled", result == 0);
    ok("lowercase content-length reaches handler once", handler_calls == 1);
    ok("lowercase content-length preserves body", strstr(response, "body=hello") != NULL);
}

static void test_invalid_content_length_rejected(void)
{
    handler_calls = 0;
    char response[1024];
    const char *request =
        "POST /echo HTTP/1.1\r\n"
        "Content-Length: five\r\n"
        "\r\n"
        "hello";

    int result = run_request(request, response, sizeof(response));
    ok("invalid content-length rejected", result == -1);
    ok("invalid content-length skips handler", handler_calls == 0);
    ok("invalid content-length returns 400", strstr(response, "HTTP/1.1 400 Bad Request") != NULL);
}

static void test_incomplete_content_length_rejected(void)
{
    handler_calls = 0;
    char response[1024];
    const char *request =
        "POST /echo HTTP/1.1\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "hello";

    int result = run_request(request, response, sizeof(response));
    ok("incomplete body rejected", result == -1);
    ok("incomplete body skips handler", handler_calls == 0);
    ok("incomplete body returns 400", strstr(response, "HTTP/1.1 400 Bad Request") != NULL);
}

static void test_oversized_content_length_rejected(void)
{
    handler_calls = 0;
    char response[1024];
    const char *request =
        "POST /echo HTTP/1.1\r\n"
        "Content-Length: 4194305\r\n"
        "\r\n";

    int result = run_request(request, response, sizeof(response));
    ok("oversized body rejected", result == -1);
    ok("oversized body skips handler", handler_calls == 0);
    ok("oversized body returns 413", strstr(response, "HTTP/1.1 413 Payload Too Large") != NULL);
}

int main(void)
{
    test_lowercase_content_length_one_packet();
    test_invalid_content_length_rejected();
    test_incomplete_content_length_rejected();
    test_oversized_content_length_rejected();

    if (failures == 0)
    {
        printf("\nALL REQUEST TESTS PASSED\n");
        return 0;
    }

    printf("\n%d REQUEST TEST(S) FAILED\n", failures);
    return 1;
}
#endif
