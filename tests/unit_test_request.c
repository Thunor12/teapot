#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
int main(void)
{
    puts("unit_test_request is POSIX-only for now");
    return 0;
}
#else
#include <sys/socket.h>
#include <unistd.h>

static int failures = 0;
static int handler_called = 0;
static size_t observed_body_length = 0;
static char observed_body[128];

static void ok(const char *name, int cond)
{
    if (cond)
        printf("[PASS] %s\n", name);
    else
    {
        printf("[FAIL] %s\n", name);
        ++failures;
    }
}

static void reset_observed(void)
{
    handler_called = 0;
    observed_body_length = 0;
    memset(observed_body, 0, sizeof(observed_body));
}

static teapot_response recording_handler(const teapot_request *req)
{
    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);

    handler_called = 1;
    observed_body_length = req->body_length;
    if (req->body.items != NULL)
    {
        size_t copy_len = req->body_length;
        if (copy_len >= sizeof(observed_body))
            copy_len = sizeof(observed_body) - 1;
        memcpy(observed_body, req->body.items, copy_len);
        observed_body[copy_len] = '\0';
    }

    teapot_response_write(&resp, "ok", 2);
    return resp;
}

static int write_all_raw(stb_teapot_socket_t fd, const char *buf, size_t len)
{
    size_t total = 0;
    while (total < len)
    {
        ssize_t n = write(fd, buf + total, len - total);
        if (n <= 0)
            return -1;
        total += (size_t)n;
    }
    return 0;
}

static size_t read_all(stb_teapot_socket_t fd, char *buf, size_t cap)
{
    size_t total = 0;
    while (total + 1 < cap)
    {
        ssize_t n = read(fd, buf + total, cap - total - 1);
        if (n <= 0)
            break;
        total += (size_t)n;
    }
    buf[total] = '\0';
    return total;
}

static int exchange_request(const char *raw_request, const teapot_route *routes, size_t route_count, char *response,
                            size_t response_cap)
{
    stb_teapot_socket_t sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
        return -1;

    if (write_all_raw(sockets[1], raw_request, strlen(raw_request)) != 0)
    {
        teapot_close(sockets[0]);
        teapot_close(sockets[1]);
        return -1;
    }
    shutdown(sockets[1], SHUT_WR);

    teapot_server server = {
        .port = 0,
        .routes = routes,
        .route_count = route_count,
    };
    int rc = teapot_handle_client_connection(&server, sockets[0]);
    (void)read_all(sockets[1], response, response_cap);
    teapot_close(sockets[1]);
    return rc;
}

static void test_lowercase_content_length_keeps_buffered_body(void)
{
    char response[512];
    teapot_route routes[] = {
        {TEAPOT_POST, "/echo", recording_handler},
    };

    reset_observed();
    int rc = exchange_request("POST /echo HTTP/1.1\r\nhost: local\r\ncontent-length: 5\r\n\r\nhello", routes,
                              sizeof(routes) / sizeof(routes[0]), response, sizeof(response));

    ok("lowercase content-length request succeeds", rc == 0);
    ok("lowercase content-length reaches handler", handler_called == 1);
    ok("lowercase content-length body length", observed_body_length == 5);
    ok("lowercase content-length body bytes", strcmp(observed_body, "hello") == 0);
    ok("lowercase content-length response is 200", strstr(response, "HTTP/1.1 200 OK") != NULL);
}

static void test_incomplete_body_rejected_before_handler(void)
{
    char response[512];
    teapot_route routes[] = {
        {TEAPOT_POST, "/echo", recording_handler},
    };

    reset_observed();
    int rc = exchange_request("POST /echo HTTP/1.1\r\nContent-Length: 10\r\n\r\nabc", routes,
                              sizeof(routes) / sizeof(routes[0]), response, sizeof(response));

    ok("incomplete body returns error", rc == -1);
    ok("incomplete body does not reach handler", handler_called == 0);
    ok("incomplete body response is 400", strstr(response, "HTTP/1.1 400 Bad Request") != NULL);
}

static void test_oversized_body_rejected_before_handler(void)
{
    char request[256];
    char response[512];
    teapot_route routes[] = {
        {TEAPOT_POST, "/echo", recording_handler},
    };

    snprintf(request, sizeof(request), "POST /echo HTTP/1.1\r\nContent-Length: %u\r\n\r\nabc",
             (unsigned int)TEAPOT_MAX_BODY_SIZE + 1u);

    reset_observed();
    int rc = exchange_request(request, routes, sizeof(routes) / sizeof(routes[0]), response, sizeof(response));

    ok("oversized body returns error", rc == -1);
    ok("oversized body does not reach handler", handler_called == 0);
    ok("oversized body response is 400", strstr(response, "HTTP/1.1 400 Bad Request") != NULL);
}

static void test_method_prefix_rejected(void)
{
    char response[512];
    teapot_route routes[] = {
        {TEAPOT_GET, "/hello", recording_handler},
    };

    reset_observed();
    int rc = exchange_request("GETTING /hello HTTP/1.1\r\n\r\n", routes, sizeof(routes) / sizeof(routes[0]), response,
                              sizeof(response));

    ok("method prefix returns error", rc == -1);
    ok("method prefix does not reach handler", handler_called == 0);
    ok("method prefix response is 400", strstr(response, "HTTP/1.1 400 Bad Request") != NULL);
}

static void test_prefix_route_matches_subpath(void)
{
    char response[512];
    teapot_route routes[] = {
        {TEAPOT_GET, "/api/", recording_handler, 1},
    };

    reset_observed();
    int rc = exchange_request("GET /api/users HTTP/1.1\r\n\r\n", routes, 1, response, sizeof(response));
    ok("prefix request succeeds", rc == 0);
    ok("prefix reaches handler", handler_called == 1);
}

static void test_exact_route_does_not_act_as_glob(void)
{
    char response[512];
    teapot_route routes[] = {
        {TEAPOT_GET, "/api/", recording_handler, 0},
    };

    reset_observed();
    int rc = exchange_request("GET /api/users HTTP/1.1\r\n\r\n", routes, 1, response, sizeof(response));
    ok("exact miss is 404", strstr(response, "HTTP/1.1 404") != NULL);
    ok("exact miss does not reach handler", handler_called == 0);
    (void)rc;
}

int main(void)
{
    printf("Running request unit tests...\n\n");

    test_lowercase_content_length_keeps_buffered_body();
    test_incomplete_body_rejected_before_handler();
    test_oversized_body_rejected_before_handler();
    test_method_prefix_rejected();
    test_prefix_route_matches_subpath();
    test_exact_route_does_not_act_as_glob();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
#endif
