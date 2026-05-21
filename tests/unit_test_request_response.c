#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
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

static size_t read_all(int fd, char *buf, size_t cap)
{
    size_t total = 0;
    if (cap == 0)
    {
        return 0;
    }

    while (total + 1 < cap)
    {
        ssize_t n = read(fd, buf + total, cap - 1 - total);
        if (n < 0)
        {
            perror("read");
            break;
        }
        if (n == 0)
        {
            break;
        }
        total += (size_t)n;
    }

    buf[total] = '\0';
    return total;
}

static void test_long_content_type_response(void)
{
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
    {
        perror("socketpair");
        ok("socketpair for long content type", 0);
        return;
    }

    const size_t content_type_len = 600;
    char *content_type = malloc(content_type_len + 1);
    if (content_type == NULL)
    {
        ok("allocate long content type", 0);
        close(sv[0]);
        close(sv[1]);
        return;
    }
    memset(content_type, 'a', content_type_len);
    content_type[content_type_len] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "ok", 2);

    int rc = teapot_send_response((stb_teapot_socket_t)sv[1], &resp);
    close(sv[1]);

    char received[2048];
    (void)read_all(sv[0], received, sizeof(received));
    close(sv[0]);

    ok("send response with long content type succeeds", rc == 0);

    const char *prefix = "Content-Type: ";
    char *content_type_header = strstr(received, prefix);
    ok("long content type header exists", content_type_header != NULL);
    if (content_type_header != NULL)
    {
        content_type_header += strlen(prefix);
        size_t observed_len = strspn(content_type_header, "a");
        ok("long content type is not truncated", observed_len == content_type_len);
        ok("long content type is followed by CRLF",
           content_type_header[observed_len] == '\r' && content_type_header[observed_len + 1] == '\n');
    }
    ok("response body follows complete header", strstr(received, "\r\n\r\nok") != NULL);

    teapot_response_free(&resp);
    free(content_type);
}

static teapot_response body_check_handler(const teapot_request *req)
{
    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_INTERNAL_ERROR);

    if (req->body_length == 5 && req->body.items != NULL && memcmp(req->body.items, "hello", 5) == 0)
    {
        resp.status = TEAPOT_HTTP_OK;
        teapot_response_write(&resp, "ok", 2);
    }
    else
    {
        tp_sb_appendf(&resp.body, "bad body length " TP_SIZE_T_FMT, req->body_length);
    }

    return resp;
}

static void test_lowercase_content_length_preserves_initial_body(void)
{
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
    {
        perror("socketpair");
        ok("socketpair for request handling", 0);
        return;
    }

    const char request[] =
        "POST /echo HTTP/1.1\r\n"
        "Host: test\r\n"
        "content-length: 5\r\n"
        "\r\n"
        "hello";
    size_t request_len = strlen(request);
    ssize_t written = write(sv[0], request, request_len);
    ok("write test request", written >= 0 && (size_t)written == request_len);
    shutdown(sv[0], SHUT_WR);

    teapot_route routes[] = {
        {TEAPOT_POST, "/echo", body_check_handler},
    };
    teapot_server server = {
        .port = 0,
        .routes = routes,
        .route_count = sizeof(routes) / sizeof(routes[0]),
    };

    int rc = teapot_handle_client_connection(&server, (stb_teapot_socket_t)sv[1]);

    char response[1024];
    (void)read_all(sv[0], response, sizeof(response));
    close(sv[0]);

    ok("handle lowercase content-length request", rc == 0);
    ok("lowercase content-length body reaches handler", strstr(response, "HTTP/1.1 200 OK\r\n") == response);
    ok("lowercase content-length response body", strstr(response, "\r\n\r\nok") != NULL);
}

int main(void)
{
    test_long_content_type_response();
    test_lowercase_content_length_preserves_initial_body();

    if (failures != 0)
    {
        printf("%d test(s) failed\n", failures);
        return 1;
    }

    printf("ALL TESTS PASSED\n");
    return 0;
}
