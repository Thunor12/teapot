#define _POSIX_C_SOURCE 200809L
#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static int failures = 0;

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

static teapot_response echo_body_handler(const teapot_request *req)
{
    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    teapot_response_write(&resp, req->body.items ? req->body.items : "", req->body_length);
    return resp;
}

static teapot_response matched_handler(const teapot_request *req)
{
    (void)req;
    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    teapot_response_write(&resp, "matched", 7);
    return resp;
}

#ifndef _WIN32
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

static int run_server_request(const teapot_route *routes, size_t route_count,
                              const char *first_write, const char *second_write,
                              char *response, size_t response_cap)
{
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
    {
        return -1;
    }

    fflush(NULL);
    pid_t child = fork();
    if (child < 0)
    {
        close(sv[0]);
        close(sv[1]);
        return -1;
    }

    if (child == 0)
    {
        close(sv[0]);
        teapot_server server = {
            .port = 0,
            .routes = routes,
            .route_count = route_count,
        };
        int rc = teapot_handle_client_connection(&server, (stb_teapot_socket_t)sv[1]);
        _exit(rc == 0 ? 0 : 2);
    }

    close(sv[1]);
    if (first_write != NULL)
    {
        size_t len = strlen(first_write);
        if (write(sv[0], first_write, len) != (ssize_t)len)
        {
            close(sv[0]);
            return -1;
        }
    }
    if (second_write != NULL)
    {
        struct timespec delay = {.tv_sec = 0, .tv_nsec = 100000000L};
        nanosleep(&delay, NULL);
        size_t len = strlen(second_write);
        if (write(sv[0], second_write, len) != (ssize_t)len)
        {
            close(sv[0]);
            return -1;
        }
    }
    shutdown(sv[0], SHUT_WR);

    read_response(sv[0], response, response_cap);
    close(sv[0]);

    int status = 0;
    waitpid(child, &status, 0);
    return 0;
}

static void test_split_headers_preserve_body(void)
{
    teapot_route routes[] = {
        {TEAPOT_POST, "/echo", echo_body_handler},
    };
    char response[1024];
    int rc = run_server_request(
        routes, sizeof(routes) / sizeof(routes[0]),
        "POST /echo HTTP/1.1\r\nContent-Type: text/plain\r\nContent-Length: 5\r\n",
        "\r\nhello",
        response, sizeof(response));

    ok("split request ran", rc == 0);
    ok("split request body preserved", strstr(response, "\r\n\r\nhello") != NULL);
    ok("split request did not consume delimiter as body", strstr(response, "\r\n\r\n\r\nhel") == NULL);
}

static void test_lowercase_content_length_preserves_buffered_body(void)
{
    teapot_route routes[] = {
        {TEAPOT_POST, "/echo", echo_body_handler},
    };
    char response[1024];
    int rc = run_server_request(
        routes, sizeof(routes) / sizeof(routes[0]),
        "POST /echo HTTP/1.1\r\nhost: local\r\ncontent-length: 5\r\n\r\nhello",
        NULL,
        response, sizeof(response));

    ok("lowercase content-length request ran", rc == 0);
    ok("lowercase content-length body preserved", strstr(response, "\r\n\r\nhello") != NULL);
}

static void test_wildcard_route_matches_subpath(void)
{
    teapot_route routes[] = {
        {TEAPOT_GET, "/api/*", matched_handler},
    };
    char response[1024];
    int rc = run_server_request(
        routes, sizeof(routes) / sizeof(routes[0]),
        "GET /api/users HTTP/1.1\r\n\r\n",
        NULL,
        response, sizeof(response));

    ok("wildcard request ran", rc == 0);
    ok("wildcard route matched subpath", strstr(response, "\r\n\r\nmatched") != NULL);
}

static void test_invalid_method_prefix_is_rejected(void)
{
    teapot_route routes[] = {
        {TEAPOT_GET, "/secure", matched_handler},
    };
    char response[1024];
    int rc = run_server_request(
        routes, sizeof(routes) / sizeof(routes[0]),
        "GETXYZ /secure HTTP/1.1\r\n\r\n",
        NULL,
        response, sizeof(response));

    ok("invalid method request ran", rc == 0);
    ok("invalid method did not route as GET", strstr(response, "matched") == NULL);
}
#endif

int main(void)
{
#ifdef _WIN32
    printf("unit_test_request is POSIX-only for socketpair coverage\n");
#else
    test_split_headers_preserve_body();
    test_lowercase_content_length_preserves_buffered_body();
    test_wildcard_route_matches_subpath();
    test_invalid_method_prefix_is_rejected();
#endif

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
