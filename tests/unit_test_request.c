#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

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
    teapot_response_write(&resp, req->body.items, req->body_length);
    return resp;
}

static void test_lowercase_lf_content_length(void)
{
    char raw[] = "POST /echo HTTP/1.1\nhost: local\ncontent-length: 4\n\nBODY";
    teapot_request req = {0};

    ok("parse lowercase LF request", parse_request(raw, strlen(raw), &req) == 0);
    ok("body length parsed", req.body_length == 4u);
    ok("body bytes parsed", req.body.items && memcmp(req.body.items, "BODY", 4u) == 0);

    free_request(&req);
}

#ifndef _WIN32
static ssize_t read_all(int fd, char *buf, size_t buf_size)
{
    size_t used = 0;
    while (used + 1u < buf_size)
    {
        ssize_t n = read(fd, buf + used, buf_size - used - 1u);
        if (n <= 0)
        {
            break;
        }
        used += (size_t)n;
    }
    buf[used] = '\0';
    return (ssize_t)used;
}

static void test_split_header_terminator(void)
{
    int sv[2] = {-1, -1};
    ok("socketpair created", socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    if (sv[0] < 0 || sv[1] < 0)
    {
        return;
    }

    teapot_route routes[] = {
        {TEAPOT_POST, "/echo", echo_body_handler},
    };
    teapot_server server = {
        .port = 0,
        .routes = routes,
        .route_count = sizeof(routes) / sizeof(routes[0]),
    };

    pid_t pid = fork();
    ok("fork server process", pid >= 0);
    if (pid == 0)
    {
        close(sv[0]);
        int rc = teapot_handle_client_connection(&server, (stb_teapot_socket_t)sv[1]);
        _exit(rc == 0 ? 0 : 1);
    }

    if (pid < 0)
    {
        close(sv[0]);
        close(sv[1]);
        return;
    }

    close(sv[1]);
    const char first[] = "POST /echo HTTP/1.1\r\nHost: local\r\nContent-Length: 5\r\n";
    const char second[] = "\r\nhello";
    ok("write split headers", write(sv[0], first, sizeof(first) - 1u) == (ssize_t)(sizeof(first) - 1u));
    usleep(20000u);
    ok("write split terminator and body", write(sv[0], second, sizeof(second) - 1u) == (ssize_t)(sizeof(second) - 1u));
    shutdown(sv[0], SHUT_WR);

    char response[1024];
    ssize_t n = read_all(sv[0], response, sizeof(response));
    ok("received split response", n > 0);
    ok("split body not corrupted", strstr(response, "\r\n\r\nhello") != NULL);

    int status = 0;
    ok("server process exited", waitpid(pid, &status, 0) == pid);
    ok("server process succeeded", WIFEXITED(status) && WEXITSTATUS(status) == 0);
    close(sv[0]);
}
#endif

int main(void)
{
    printf("Running request unit tests...\n\n");

    test_lowercase_lf_content_length();
#ifndef _WIN32
    test_split_header_terminator();
#else
    printf("[SKIP] socketpair-based tests are POSIX-only\n");
#endif

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
