#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

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

static ssize_t read_all_fd(int fd, char *buf, size_t cap)
{
    size_t total = 0;
    while (total + 1 < cap)
    {
        ssize_t n = read(fd, buf + total, cap - total - 1);
        if (n < 0)
        {
            return -1;
        }
        if (n == 0)
        {
            break;
        }
        total += (size_t)n;
    }
    buf[total] = '\0';
    return (ssize_t)total;
}

static void short_pause(void)
{
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 50 * 1000 * 1000;
    nanosleep(&ts, NULL);
}

static teapot_response echo_handler(const teapot_request *req)
{
    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    tp_sb_appendf(&resp.body, "len=%zu body=", req->body_length);
    teapot_response_write(&resp, req->body.items, req->body_length);
    return resp;
}

static int run_exchange(
    const char *first, size_t first_len,
    const char *second, size_t second_len,
    char *response, size_t response_cap,
    int *child_exit)
{
    int sv[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
    {
        return -1;
    }

    if (first_len > 0 && write_all_fd(sv[1], first, first_len) != 0)
    {
        close(sv[0]);
        close(sv[1]);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        close(sv[0]);
        close(sv[1]);
        return -1;
    }

    if (pid == 0)
    {
        close(sv[1]);
        teapot_route routes[] = {
            {TEAPOT_POST, "/echo", echo_handler},
        };
        teapot_server server = {
            .port = 0,
            .routes = routes,
            .route_count = sizeof(routes) / sizeof(routes[0]),
        };
        int ret = teapot_handle_client_connection(&server, (stb_teapot_socket_t)sv[0]);
        _exit(ret == 0 ? 0 : 10);
    }

    close(sv[0]);
    if (second_len > 0)
    {
        short_pause();
        if (write_all_fd(sv[1], second, second_len) != 0)
        {
            close(sv[1]);
            return -1;
        }
    }
    shutdown(sv[1], SHUT_WR);

    if (read_all_fd(sv[1], response, response_cap) < 0)
    {
        close(sv[1]);
        return -1;
    }
    close(sv[1]);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
    {
        return -1;
    }
    if (child_exit)
    {
        *child_exit = WIFEXITED(status) ? WEXITSTATUS(status) : 255;
    }

    return 0;
}

static void test_lowercase_content_length_keeps_buffered_body(void)
{
    const char req[] =
        "POST /echo HTTP/1.1\r\n"
        "host: example.test\r\n"
        "content-length: 5\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "hello";
    char response[512];
    int child_exit = -1;

    ok("lowercase Content-Length exchange", run_exchange(req, strlen(req), NULL, 0, response, sizeof(response), &child_exit) == 0);
    ok("lowercase Content-Length handled", child_exit == 0);
    ok("lowercase Content-Length body preserved", strstr(response, "len=5 body=hello") != NULL);
}

static void test_lf_only_header_end_keeps_buffered_body(void)
{
    const char req[] =
        "POST /echo HTTP/1.1\n"
        "Content-Length: 5\n"
        "Content-Type: text/plain\n"
        "\n"
        "hello";
    char response[512];
    int child_exit = -1;

    ok("LF-only request exchange", run_exchange(req, strlen(req), NULL, 0, response, sizeof(response), &child_exit) == 0);
    ok("LF-only request handled", child_exit == 0);
    ok("LF-only body preserved", strstr(response, "len=5 body=hello") != NULL);
}

static void test_split_header_body_boundary(void)
{
    const char first[] =
        "POST /echo HTTP/1.1\r\n"
        "Content-Length: 5\r\n"
        "Content-Type: text/plain\r\n";
    const char second[] = "\r\nhello";
    char response[512];
    int child_exit = -1;

    ok("split header exchange", run_exchange(first, strlen(first), second, strlen(second), response, sizeof(response), &child_exit) == 0);
    ok("split header request handled", child_exit == 0);
    ok("split header body preserved", strstr(response, "len=5 body=hello") != NULL);
}

static void test_incomplete_body_is_rejected(void)
{
    const char req[] =
        "POST /echo HTTP/1.1\r\n"
        "Content-Length: 5\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "hel";
    char response[512];
    int child_exit = -1;

    ok("incomplete body exchange", run_exchange(req, strlen(req), NULL, 0, response, sizeof(response), &child_exit) == 0);
    ok("incomplete body rejected", child_exit != 0);
    ok("incomplete body produced no handler response", response[0] == '\0');
}

int main(void)
{
    printf("Running request unit tests...\n\n");

    test_lowercase_content_length_keeps_buffered_body();
    test_lf_only_header_end_keeps_buffered_body();
    test_split_header_body_boundary();
    test_incomplete_body_is_rejected();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
