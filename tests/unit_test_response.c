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

static int send_and_read(teapot_response *r, char *buf, size_t cap)
{
    stb_teapot_socket_t sp[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp) != 0)
        return -1;
    int rc = teapot_send_response(sp[0], r);
    shutdown(sp[0], SHUT_WR);
    if (rc == 0)
        (void)read_all(sp[1], buf, cap);
    else
        buf[0] = '\0';
    teapot_close(sp[0]);
    teapot_close(sp[1]);
    return rc;
}

static void test_header_rejects_empty_name_and_crlf(void)
{
    teapot_response r;
    teapot_response_init(&r, TEAPOT_HTTP_OK);
    ok("reject empty name", teapot_response_header(&r, "", "v") == -1);
    ok("reject CR in name", teapot_response_header(&r, "X-\rName", "v") == -1);
    ok("reject LF in name", teapot_response_header(&r, "X-\nName", "v") == -1);
    ok("reject CR in value", teapot_response_header(&r, "X-Ok", "a\rb") == -1);
    ok("reject LF in value", teapot_response_header(&r, "X-Ok", "a\nb") == -1);
    teapot_response_free(&r);
}

static void test_header_rejects_colon_space_ctl_in_name(void)
{
    teapot_response r;
    teapot_response_init(&r, TEAPOT_HTTP_OK);
    ok("reject colon in name", teapot_response_header(&r, "X:Bad", "v") == -1);
    ok("reject space in name", teapot_response_header(&r, "X Bad", "v") == -1);
    ok("reject CTL in name", teapot_response_header(&r, "X-" "\x01" "Bad", "v") == -1);
    ok("reject DEL in name", teapot_response_header(&r, "X-" "\x7F" "Bad", "v") == -1);
    ok("reject CTL in value", teapot_response_header(&r, "X-Ok", "a" "\x01" "b") == -1);
    ok("reject DEL in value", teapot_response_header(&r, "X-Ok", "a" "\x7F" "b") == -1);
    ok("allow HTAB in value", teapot_response_header(&r, "X-Tab", "a\tb") == 0);
    teapot_response_free(&r);
}

static void test_header_rejects_oversize(void)
{
    teapot_response r;
    teapot_response_init(&r, TEAPOT_HTTP_OK);

    char name[TP_MAX_HEADER_NAME_LEN + 2];
    memset(name, 'N', sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    ok("reject oversize name", teapot_response_header(&r, name, "v") == -1);

    char value[TP_MAX_HEADER_VALUE_LEN + 2];
    memset(value, 'V', sizeof(value) - 1);
    value[sizeof(value) - 1] = '\0';
    ok("reject oversize value", teapot_response_header(&r, "X-Ok", value) == -1);
    teapot_response_free(&r);
}

static void test_header_empty_value_on_wire(void)
{
    teapot_response r = teapot_text(TEAPOT_HTTP_OK, "OK");
    ok("empty value accepted", teapot_response_header(&r, "X-Empty", "") == 0);

    char received[1024];
    ok("send empty value", send_and_read(&r, received, sizeof(received)) == 0);
    ok("empty value present on wire", strstr(received, "X-Empty: \r\n") != NULL || strstr(received, "X-Empty:\r\n") != NULL);
    teapot_response_free(&r);
}

static void test_header_reserved_and_format_skip(void)
{
    teapot_response r;
    teapot_response_init(&r, TEAPOT_HTTP_OK);
    r.content_type = "text/plain";
    teapot_response_write(&r, "OK", 2);

    ok("reject Content-Type", teapot_response_header(&r, "Content-Type", "text/html") == -1);
    ok("reject content-length", teapot_response_header(&r, "content-length", "999") == -1);
    ok("reject CONNECTION", teapot_response_header(&r, "CONNECTION", "keep-alive") == -1);
    ok("reject Transfer-Encoding", teapot_response_header(&r, "Transfer-Encoding", "chunked") == -1);

    /* Force reserved into headers; format must omit. */
    tp_header_line forced = {0};
    tp_sb_append_cstr(&forced.name, "Content-Length");
    tp_sb_append_null(&forced.name);
    tp_sb_append_cstr(&forced.value, "999");
    tp_sb_append_null(&forced.value);
    tp_da_append(&r.headers, forced);

    char received[1024];
    ok("send with forced reserved", send_and_read(&r, received, sizeof(received)) == 0);
    ok("format keeps real content-length 2", strstr(received, "Content-Length: 2\r\n") != NULL);
    ok("format omits forced reserved 999", strstr(received, "Content-Length: 999") == NULL);
    teapot_response_free(&r);
}

static void test_two_set_cookie_append_order(void)
{
    teapot_response r = teapot_text(TEAPOT_HTTP_OK, "OK");
    ok("set-cookie a", teapot_response_header(&r, "Set-Cookie", "a=1") == 0);
    ok("set-cookie b", teapot_response_header(&r, "Set-Cookie", "b=2") == 0);

    char received[1024];
    ok("send cookies", send_and_read(&r, received, sizeof(received)) == 0);
    const char *a = strstr(received, "Set-Cookie: a=1\r\n");
    const char *b = strstr(received, "Set-Cookie: b=2\r\n");
    ok("both set-cookie on wire", a != NULL && b != NULL);
    ok("set-cookie append order", a != NULL && b != NULL && a < b);
    teapot_response_free(&r);
}

static void test_teapot_html(void)
{
    teapot_response r = teapot_html(TEAPOT_HTTP_OK, "<p>hi</p>");
    ok("html ctype", r.content_type && strcmp(r.content_type, "text/html; charset=utf-8") == 0);
    ok("html body", r.body.count == 9 && memcmp(r.body.items, "<p>hi</p>", 9) == 0);
    ok("html headers zeroed", r.headers.count == 0 && r.headers.items == NULL);
    teapot_response_free(&r);

    teapot_response empty = teapot_html(TEAPOT_HTTP_OK, NULL);
    ok("html NULL empty body", empty.body.count == 0 && empty.body.items == NULL);
    teapot_response_free(&empty);
}

static void test_empty_headers_byte_identical_to_text_ok(void)
{
    const char expected[] =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 2\r\n"
        "Connection: close\r\n"
        "\r\n"
        "OK";

    teapot_response r = teapot_text(TEAPOT_HTTP_OK, "OK");
    char received[1024];
    ok("send text OK", send_and_read(&r, received, sizeof(received)) == 0);
    ok("byte-identical empty headers wire",
       strlen(received) == sizeof(expected) - 1 && memcmp(received, expected, sizeof(expected) - 1) == 0);
    teapot_response_free(&r);
}

int main(void)
{
    printf("Running response unit tests...\n\n");
    test_long_content_type_response();
    test_rejects_response_header_injection();
    test_teapot_json_sets_ctype_and_body();
    test_teapot_text_sets_ctype_and_body();
    test_teapot_bytes_crlf_ctype_rejected_on_send();
    test_header_rejects_empty_name_and_crlf();
    test_header_rejects_colon_space_ctl_in_name();
    test_header_rejects_oversize();
    test_header_empty_value_on_wire();
    test_header_reserved_and_format_skip();
    test_two_set_cookie_append_order();
    test_teapot_html();
    test_empty_headers_byte_identical_to_text_ok();
    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d TEST(S) FAILED\n", failures);
    return 1;
}
#endif
