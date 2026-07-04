#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#ifndef _WIN32
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

static int write_all(int fd, const char *buf, size_t len)
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

static size_t read_all(int fd, char *buf, size_t cap)
{
    size_t used = 0;
    while (used + 1 < cap)
    {
        ssize_t n = read(fd, buf + used, cap - used - 1);
        if (n <= 0)
        {
            break;
        }
        used += (size_t)n;
    }
    buf[used] = '\0';
    return used;
}

static teapot_response echo_handler(const teapot_request *req)
{
    ++handler_calls;
    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    tp_sb_appendf(&resp.body, "LEN=" TP_SIZE_T_FMT " BODY=%s", req->body_length, req->body.items ? req->body.items : "");
    return resp;
}

static int round_trip(const char *request, char *response, size_t response_cap)
{
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
    {
        return -1;
    }

    teapot_route routes[] = {
        {TEAPOT_POST, "/echo", echo_handler},
        {TEAPOT_GET, "/api/*", echo_handler},
    };
    teapot_server server = {
        .port = 0,
        .routes = routes,
        .route_count = sizeof(routes) / sizeof(routes[0]),
    };

    if (write_all(sv[1], request, strlen(request)) != 0)
    {
        close(sv[0]);
        close(sv[1]);
        return -1;
    }
    shutdown(sv[1], SHUT_WR);

    int rc = teapot_handle_client_connection(&server, sv[0]);
    read_all(sv[1], response, response_cap);
    close(sv[1]);
    return rc;
}

static void test_lowercase_content_length_uses_buffered_body(void)
{
    handler_calls = 0;
    char response[512];
    int rc = round_trip("POST /echo HTTP/1.1\r\ncontent-length: 5\r\n\r\nhello", response, sizeof(response));
    ok("lowercase content-length handled", rc == 0);
    ok("handler called once", handler_calls == 1);
    ok("body preserved", strstr(response, "LEN=5 BODY=hello") != NULL);
}

static void test_oversized_body_rejected_before_handler(void)
{
    handler_calls = 0;
    char response[512];
    int rc = round_trip("POST /echo HTTP/1.1\r\nContent-Length: 4194305\r\n\r\n", response, sizeof(response));
    ok("oversized body rejected", rc < 0);
    ok("oversized body skips handler", handler_calls == 0);
    ok("oversized body returns 413", strstr(response, "HTTP/1.1 413 Payload Too Large") != NULL);
}

static void test_incomplete_body_rejected_before_handler(void)
{
    handler_calls = 0;
    char response[512];
    int rc = round_trip("POST /echo HTTP/1.1\r\nContent-Length: 5\r\n\r\nhe", response, sizeof(response));
    ok("incomplete body rejected", rc < 0);
    ok("incomplete body skips handler", handler_calls == 0);
    ok("incomplete body returns 400", strstr(response, "HTTP/1.1 400 Bad Request") != NULL);
}

static void test_method_must_match_exactly(void)
{
    handler_calls = 0;
    char response[512];
    int rc = round_trip("POSTX /echo HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello", response, sizeof(response));
    ok("invalid method rejected", rc < 0);
    ok("invalid method skips handler", handler_calls == 0);
    ok("invalid method returns 400", strstr(response, "HTTP/1.1 400 Bad Request") != NULL);
}

static void test_wildcard_route_matches_subpath(void)
{
    handler_calls = 0;
    char response[512];
    int rc = round_trip("GET /api/health HTTP/1.1\r\n\r\n", response, sizeof(response));
    ok("wildcard route subpath accepted", rc == 0);
    ok("wildcard handler called", handler_calls == 1);
    ok("wildcard route returned 200", strstr(response, "HTTP/1.1 200 OK") != NULL);
}

int main(void)
{
    test_lowercase_content_length_uses_buffered_body();
    test_oversized_body_rejected_before_handler();
    test_incomplete_body_rejected_before_handler();
    test_method_must_match_exactly();
    test_wildcard_route_matches_subpath();

    if (failures != 0)
    {
        printf("%d request test(s) failed\n", failures);
        return 1;
    }

    printf("request tests passed\n");
    return 0;
}
#else
int main(void)
{
    return 0;
}
#endif
