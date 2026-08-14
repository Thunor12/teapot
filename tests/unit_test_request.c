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
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
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

static void test_oversize_header_rejected_before_handler(void)
{
    char name[TP_MAX_HEADER_NAME_LEN + 12];
    memset(name, 'N', sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';

    char request[TP_MAX_HEADER_NAME_LEN + 64];
    snprintf(request, sizeof(request), "GET /hello HTTP/1.1\r\n%s: v\r\n\r\n", name);

    char response[512];
    teapot_route routes[] = {
        {TEAPOT_GET, "/hello", recording_handler},
    };

    reset_observed();
    int rc = exchange_request(request, routes, 1, response, sizeof(response));
    ok("oversize header returns error", rc == -1);
    ok("oversize header does not reach handler", handler_called == 0);
    ok("oversize header response is 400", strstr(response, "HTTP/1.1 400 Bad Request") != NULL);
}

struct split_serve_ctx
{
    teapot_server *server;
    stb_teapot_socket_t fd;
    int rc;
};

static void *split_serve_thread(void *arg)
{
    struct split_serve_ctx *ctx = arg;
    ctx->rc = teapot_serve_client(ctx->server, ctx->fd);
    return NULL;
}

static void test_split_body_reads_remaining_bytes(void)
{
    stb_teapot_socket_t sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
    {
        ok("split body socketpair", 0);
        return;
    }

    const char *first = "POST /echo HTTP/1.1\r\nContent-Length: 5\r\n\r\nhel";
    if (write_all_raw(sockets[1], first, strlen(first)) != 0)
    {
        ok("split body wrote headers and partial body", 0);
        teapot_close(sockets[0]);
        teapot_close(sockets[1]);
        return;
    }

    teapot_route routes[] = {
        {TEAPOT_POST, "/echo", recording_handler},
    };
    teapot_server server = {
        .port = 0,
        .routes = routes,
        .route_count = 1,
    };
    struct split_serve_ctx ctx = {
        .server = &server,
        .fd = sockets[0],
        .rc = 1,
    };

    reset_observed();
    pthread_t th;
    if (pthread_create(&th, NULL, split_serve_thread, &ctx) != 0)
    {
        ok("split body thread", 0);
        teapot_close(sockets[0]);
        teapot_close(sockets[1]);
        return;
    }

    int drained = 0;
    for (int i = 0; i < 2000; ++i)
    {
        int unread = 0;
        if (ioctl(sockets[0], FIONREAD, &unread) == 0 && unread == 0)
        {
            drained = 1;
            break;
        }
        {
            struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000000L};
            nanosleep(&ts, NULL);
        }
    }
    ok("split body first write drained", drained);
    ok("split body wrote remainder", write_all_raw(sockets[1], "lo", 2) == 0);
    pthread_join(th, NULL);

    ok("split body serve succeeds", ctx.rc == 0);
    ok("split body reaches handler", handler_called == 1);
    ok("split body length", observed_body_length == 5);
    ok("split body bytes", strcmp(observed_body, "hello") == 0);

    teapot_close(sockets[0]);
    teapot_close(sockets[1]);
}

static void test_serve_client_leaves_fd_open(void)
{
    stb_teapot_socket_t sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
    {
        ok("serve_client fd socketpair", 0);
        return;
    }

    if (write_all_raw(sockets[1], "GET /hello HTTP/1.1\r\n\r\n", strlen("GET /hello HTTP/1.1\r\n\r\n")) != 0)
    {
        ok("serve_client fd wrote request", 0);
        teapot_close(sockets[0]);
        teapot_close(sockets[1]);
        return;
    }
    shutdown(sockets[1], SHUT_WR);

    teapot_route routes[] = {
        {TEAPOT_GET, "/hello", recording_handler},
    };
    teapot_server server = {
        .port = 0,
        .routes = routes,
        .route_count = 1,
    };

    reset_observed();
    int rc = teapot_serve_client(&server, sockets[0]);
    ok("serve_client succeeds", rc == 0);
    ok("serve_client reaches handler", handler_called == 1);
    ok("serve_client leaves client fd open", write(sockets[0], "x", 1) == 1);

    teapot_close(sockets[0]);
    teapot_close(sockets[1]);
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

static void test_prefix_without_slash_does_not_match_extension(void)
{
    char response[512];
    teapot_route routes[] = {
        {TEAPOT_GET, "/api", recording_handler, 1},
    };

    reset_observed();
    int rc = exchange_request("GET /apiary HTTP/1.1\r\n\r\n", routes, 1, response, sizeof(response));
    ok("bare prefix does not match /apiary", strstr(response, "HTTP/1.1 404") != NULL);
    ok("bare prefix /apiary does not reach handler", handler_called == 0);
    (void)rc;
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
    test_oversize_header_rejected_before_handler();
    test_split_body_reads_remaining_bytes();
    test_serve_client_leaves_fd_open();
    test_prefix_route_matches_subpath();
    test_prefix_without_slash_does_not_match_extension();
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
