#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
int main(void)
{
    puts("unit_test_response is POSIX-only for now");
    return 0;
}
#else
#include <sys/socket.h>
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

static void test_long_content_type_response(void)
{
    stb_teapot_socket_t sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
    {
        ok("socketpair for long content type", 0);
        return;
    }

    char content_type[400];
    memset(content_type, 'a', sizeof(content_type) - 1);
    content_type[sizeof(content_type) - 1] = '\0';

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = content_type;
    teapot_response_write(&resp, "OK", 2);

    ok("send long content type", teapot_send_response(sockets[0], &resp) == 0);
    shutdown(sockets[0], SHUT_WR);

    char received[1024];
    (void)read_all(sockets[1], received, sizeof(received));
    ok("response includes full long content type", strstr(received, content_type) != NULL);
    ok("response includes body length", strstr(received, "Content-Length: 2\r\n") != NULL);
    ok("response includes connection close", strstr(received, "Connection: close\r\n") != NULL);
    ok("response ends headers before body", strstr(received, "\r\n\r\nOK") != NULL);

    teapot_response_free(&resp);
    teapot_close(sockets[0]);
    teapot_close(sockets[1]);
}

static void test_rejects_response_header_injection(void)
{
    stb_teapot_socket_t sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
    {
        ok("socketpair for injection", 0);
        return;
    }

    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    resp.content_type = "text/plain\r\nX-Injected: yes";
    teapot_response_write(&resp, "OK", 2);

    ok("reject CRLF in content type", teapot_send_response(sockets[0], &resp) == -1);

    teapot_response_free(&resp);
    teapot_close(sockets[0]);
    teapot_close(sockets[1]);
}

static void test_teapot_json_sets_ctype_and_body(void)
{
    teapot_response r = teapot_json(TEAPOT_HTTP_OK, "{\"ok\":true}");
    ok("json ctype", r.content_type && strcmp(r.content_type, "application/json") == 0);
    ok("json body", r.body.count == 11 && memcmp(r.body.items, "{\"ok\":true}", 11) == 0);
    teapot_response_free(&r);
}

static void test_teapot_text_sets_ctype_and_body(void)
{
    teapot_response r = teapot_text(TEAPOT_HTTP_OK, "hello");
    ok("text ctype", r.content_type && strcmp(r.content_type, "text/plain") == 0);
    ok("text body", r.body.count == 5 && memcmp(r.body.items, "hello", 5) == 0);
    teapot_response_free(&r);
}

static void test_teapot_bytes_crlf_ctype_rejected_on_send(void)
{
    stb_teapot_socket_t sp[2];
    ok("socketpair", socketpair(AF_UNIX, SOCK_STREAM, 0, sp) == 0);
    teapot_response r = teapot_bytes(TEAPOT_HTTP_OK, "a\r\nX: b", "x", 1);
    ok("send rejects", teapot_send_response(sp[0], &r) == -1);
    teapot_response_free(&r);
    teapot_close(sp[0]);
    teapot_close(sp[1]);
}

int main(void)
{
    printf("Running response unit tests...\n\n");
    test_long_content_type_response();
    test_rejects_response_header_injection();
    test_teapot_json_sets_ctype_and_body();
    test_teapot_text_sets_ctype_and_body();
    test_teapot_bytes_crlf_ctype_rejected_on_send();
    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
#endif
