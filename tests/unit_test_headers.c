#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* tiny test helpers */
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

static char *mkbuf(const char *s)
{
    char *ret = NULL;
    if (s)
    {
        ret = malloc(strlen(s) + 1);
        strcpy(ret, s);
    }

    return ret;
}

static const tp_header_line *get_line(const tp_headers *h, size_t idx)
{
    if (!h || idx >= h->count)
        return NULL;
    return &h->items[idx];
}

static void test_empty(void)
{
    tp_headers h = {0};
    char *buf = mkbuf("");
    tp_extract_header_keyval(&h, buf, strlen(buf));
    ok("empty -> 0 headers", h.count == 0);
    tp_headers_free(&h);
    free(buf);
}

static void test_simple(void)
{
    tp_headers h = {0};
    char *buf = mkbuf("Host: example.com\r\n");
    tp_extract_header_keyval(&h, buf, strlen(buf));
    ok("simple -> 1 header", h.count == 1);
    const tp_header_line *hl = get_line(&h, 0);
    ok("simple name == Host", hl && hl->name.items && strcmp(hl->name.items, "Host") == 0);
    ok("simple value == example.com", hl && hl->value.items && strcmp(hl->value.items, "example.com") == 0);
    tp_headers_free(&h);
    free(buf);
}

static void test_trim_spaces(void)
{
    tp_headers h = {0};
    char *buf = mkbuf("  X-Hello  :   world  \r\n");
    tp_extract_header_keyval(&h, buf, strlen(buf));
    ok("trim -> 1 header", h.count == 1);
    const tp_header_line *hl = get_line(&h, 0);
    ok("trim name == X-Hello", hl && hl->name.items && strcmp(hl->name.items, "X-Hello") == 0);
    ok("trim value == world", hl && hl->value.items && strcmp(hl->value.items, "world") == 0);
    tp_headers_free(&h);
    free(buf);
}

static void test_crlf_and_lf(void)
{
    tp_headers h = {0};
    char *buf = mkbuf("A:1\r\nB:2\n\n");
    tp_extract_header_keyval(&h, buf, strlen(buf));
    ok("crlf/lf -> 2 headers", h.count == 2);
    const tp_header_line *a = get_line(&h, 0);
    const tp_header_line *b = get_line(&h, 1);
    ok("A==1", a && a->value.items && strcmp(a->value.items, "1") == 0);
    ok("B==2", b && b->value.items && strcmp(b->value.items, "2") == 0);
    tp_headers_free(&h);
    free(buf);
}

static void test_repeated_headers(void)
{
    tp_headers h = {0};
    char *buf = mkbuf("Set-Cookie: a=1\r\nSet-Cookie: b=2\r\n");
    tp_extract_header_keyval(&h, buf, strlen(buf));
    ok("repeated -> 2 headers", h.count == 2);
    const tp_header_line *h0 = get_line(&h, 0);
    const tp_header_line *h1 = get_line(&h, 1);
    ok("first cookie a=1", h0 && h0->value.items && strcmp(h0->value.items, "a=1") == 0);
    ok("second cookie b=2", h1 && h1->value.items && strcmp(h1->value.items, "b=2") == 0);
    tp_headers_free(&h);
    free(buf);
}

static void test_no_colon_ignored(void)
{
    tp_headers h = {0};
    char *buf = mkbuf("NoColonLine\r\nX: y\r\n");
    tp_extract_header_keyval(&h, buf, strlen(buf));
    ok("no-colon ignored -> 1 header", h.count == 1);
    const tp_header_line *hl = get_line(&h, 0);
    ok("X==y", hl && hl->value.items && strcmp(hl->value.items, "y") == 0);
    tp_headers_free(&h);
    free(buf);
}

static void test_empty_name_ignored(void)
{
    tp_headers h = {0};
    char *buf = mkbuf(": value\r\nGood: v\r\n");
    tp_extract_header_keyval(&h, buf, strlen(buf));
    ok("empty name ignored -> 1 header", h.count == 1);
    const tp_header_line *hl = get_line(&h, 0);
    ok("Good==v", hl && hl->name.items && strcmp(hl->name.items, "Good") == 0 && hl->value.items && strcmp(hl->value.items, "v") == 0);
    tp_headers_free(&h);
    free(buf);
}

static void test_clamping(void)
{
    tp_headers h = {0};

    /* build a name longer than TP_MAX_HEADER_NAME_LEN and a value longer than TP_MAX_HEADER_VALUE_LEN */
#ifndef TP_MAX_HEADER_NAME_LEN
#define TP_MAX_HEADER_NAME_LEN 256
#endif
#ifndef TP_MAX_HEADER_VALUE_LEN
#define TP_MAX_HEADER_VALUE_LEN 4096
#endif

    size_t long_name_len = (size_t)TP_MAX_HEADER_NAME_LEN + 10;
    size_t long_val_len = (size_t)TP_MAX_HEADER_VALUE_LEN + 20;

    char *long_name = malloc(long_name_len + 2);
    char *long_val = malloc(long_val_len + 2);
    for (size_t i = 0; i < long_name_len; ++i)
        long_name[i] = 'N';
    for (size_t i = 0; i < long_val_len; ++i)
        long_val[i] = 'V';
    long_name[long_name_len] = '\0';
    long_val[long_val_len] = '\0';

    size_t buflen = long_name_len + 2 + long_val_len + 4;
    char *buf = malloc(buflen + 1);
    snprintf(buf, buflen + 1, "%s: %s\r\n", long_name, long_val);

    tp_extract_header_keyval(&h, buf, strlen(buf));
    ok("clamping -> parsed 1 header", h.count == 1);
    const tp_header_line *hl = get_line(&h, 0);
    size_t parsed_name_len = hl && hl->name.items ? strlen(hl->name.items) : 0;
    size_t parsed_val_len = hl && hl->value.items ? strlen(hl->value.items) : 0;
    ok("name clamped", parsed_name_len <= (size_t)TP_MAX_HEADER_NAME_LEN);
    ok("value clamped", parsed_val_len <= (size_t)TP_MAX_HEADER_VALUE_LEN);

    tp_headers_free(&h);
    free(buf);
    free(long_name);
    free(long_val);
}

int main(void)
{
    printf("Running header parsing unit tests...\n\n");

    test_empty();
    test_simple();
    test_trim_spaces();
    test_crlf_and_lf();
    test_repeated_headers();
    test_no_colon_ignored();
    test_empty_name_ignored();
    test_clamping();

    if (failures == 0)
    {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    else
    {
        printf("\n%d TEST(S) FAILED\n", failures);
        return 1;
    }
}
