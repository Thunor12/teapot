#define _POSIX_C_SOURCE 200809L
#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
int main(void)
{
    return 0;
}
#else
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int failures = 0;

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

static teapot_response validate_body_handler(const teapot_request *req)
{
    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    if (req->body_length == 5 && req->body.items && memcmp(req->body.items, "hello", 5) == 0)
    {
        resp.status = TEAPOT_HTTP_CREATED;
        teapot_response_write(&resp, "ok", 2);
    }
    else
    {
        resp.status = TEAPOT_HTTP_INTERNAL_ERROR;
        teapot_response_write(&resp, req->body.items ? req->body.items : "", req->body_length);
    }
    return resp;
}

static teapot_response unexpected_handler(const teapot_request *req)
{
    (void)req;
    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_INTERNAL_ERROR);
    teapot_response_write(&resp, "handler called", 14);
    return resp;
}

static ssize_t read_response(int fd, char *buf, size_t cap)
{
    ssize_t total = 0;
    while ((size_t)total + 1 < cap)
    {
        ssize_t n = read(fd, buf + (size_t)total, cap - (size_t)total - 1);
        if (n <= 0)
            break;
        total += n;
    }
    buf[total > 0 ? (size_t)total : 0] = '\0';
    return total;
}

static int write_all_fd(int fd, const char *buf, size_t len)
{
    size_t written = 0;
    while (written < len)
    {
        ssize_t n = write(fd, buf + written, len - written);
        if (n <= 0)
            return -1;
        written += (size_t)n;
    }
    return 0;
}

static int run_server_once(teapot_route *routes, size_t route_count, int server_fd)
{
    teapot_server server = {
        .port = 0,
        .routes = routes,
        .route_count = route_count,
    };
    return teapot_handle_client_connection(&server, server_fd);
}

static void test_split_headers_body_stays_intact(void)
{
    int fds[2] = {-1, -1};
    ok("socketpair for split headers", socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    if (fds[0] < 0 || fds[1] < 0)
        return;

    pid_t pid = fork();
    ok("fork split header server", pid >= 0);
    if (pid < 0)
    {
        close(fds[0]);
        close(fds[1]);
        return;
    }
    if (pid == 0)
    {
        close(fds[0]);
        teapot_route routes[] = {
            {TEAPOT_POST, "/save", validate_body_handler},
        };
        int rc = run_server_once(routes, sizeof(routes) / sizeof(routes[0]), fds[1]);
        _exit(rc == 0 ? 0 : 1);
    }

    close(fds[1]);
    const char *part1 =
        "POST /save HTTP/1.1\r\n"
        "Host: example\r\n"
        "content-length: 5\r\n"
        "Content-Type: text/plain\r\n";
    const char *part2 = "\r\nhello";
    ok("write split header part 1", write_all_fd(fds[0], part1, strlen(part1)) == 0);
    {
        struct timespec ts = {0, 10000000L};
        nanosleep(&ts, NULL);
    }
    ok("write split header part 2", write_all_fd(fds[0], part2, strlen(part2)) == 0);
    shutdown(fds[0], SHUT_WR);

    char response[512];
    ssize_t n = read_response(fds[0], response, sizeof(response));
    ok("read split header response", n > 0);
    ok("split body accepted as created", strstr(response, "HTTP/1.1 201 Created") != NULL);

    int status = 0;
    waitpid(pid, &status, 0);
    ok("split header child exited cleanly", WIFEXITED(status) && WEXITSTATUS(status) == 0);
    close(fds[0]);
}

static void test_incomplete_body_rejected_before_handler(void)
{
    int fds[2] = {-1, -1};
    ok("socketpair for incomplete body", socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    if (fds[0] < 0 || fds[1] < 0)
        return;

    pid_t pid = fork();
    ok("fork incomplete body server", pid >= 0);
    if (pid < 0)
    {
        close(fds[0]);
        close(fds[1]);
        return;
    }
    if (pid == 0)
    {
        close(fds[0]);
        teapot_route routes[] = {
            {TEAPOT_POST, "/save", unexpected_handler},
        };
        (void)run_server_once(routes, sizeof(routes) / sizeof(routes[0]), fds[1]);
        _exit(0);
    }

    close(fds[1]);
    const char *request =
        "POST /save HTTP/1.1\r\n"
        "Host: example\r\n"
        "Content-Length: 5\r\n"
        "\r\n"
        "he";
    ok("write incomplete body request", write_all_fd(fds[0], request, strlen(request)) == 0);
    shutdown(fds[0], SHUT_WR);

    char response[512];
    ssize_t n = read_response(fds[0], response, sizeof(response));
    ok("read incomplete body response", n > 0);
    ok("incomplete body rejected", strstr(response, "HTTP/1.1 400 Bad Request") != NULL);
    ok("incomplete body did not call handler", strstr(response, "handler called") == NULL);

    int status = 0;
    waitpid(pid, &status, 0);
    ok("incomplete body child exited", WIFEXITED(status));
    close(fds[0]);
}

static void test_method_token_must_match_exactly(void)
{
    int fds[2] = {-1, -1};
    ok("socketpair for method test", socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    if (fds[0] < 0 || fds[1] < 0)
        return;

    pid_t pid = fork();
    ok("fork method server", pid >= 0);
    if (pid < 0)
    {
        close(fds[0]);
        close(fds[1]);
        return;
    }
    if (pid == 0)
    {
        close(fds[0]);
        teapot_route routes[] = {
            {TEAPOT_GET, "/secret", unexpected_handler},
        };
        (void)run_server_once(routes, sizeof(routes) / sizeof(routes[0]), fds[1]);
        _exit(0);
    }

    close(fds[1]);
    const char *request = "GETTING /secret HTTP/1.1\r\nHost: example\r\n\r\n";
    ok("write invalid method request", write_all_fd(fds[0], request, strlen(request)) == 0);
    shutdown(fds[0], SHUT_WR);

    char response[512];
    ssize_t n = read_response(fds[0], response, sizeof(response));
    ok("read invalid method response", n > 0);
    ok("invalid method rejected", strstr(response, "HTTP/1.1 400 Bad Request") != NULL);
    ok("invalid method did not call GET handler", strstr(response, "handler called") == NULL);

    int status = 0;
    waitpid(pid, &status, 0);
    ok("method child exited", WIFEXITED(status));
    close(fds[0]);
}

int main(void)
{
    test_split_headers_body_stays_intact();
    test_incomplete_body_rejected_before_handler();
    test_method_token_must_match_exactly();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }

    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
#endif
