#include "../build/test_embed.h"

#include <stdio.h>
#include <string.h>

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

static void test_find_hit(void)
{
    const tp_embed_file *f = tp_embed_find("/index.html");
    ok("find /index.html", f != NULL);
    ok("index len", f && f->len == 36);
    ok("index first bytes", f && f->len >= 2 && f->data[0] == '<' && f->data[1] == '!');
    ok("index ctype", f && strcmp(f->ctype, "text/html; charset=utf-8") == 0);
}

static void test_find_query_strip(void)
{
    const tp_embed_file *f = tp_embed_find("/index.html?x=1");
    ok("find with query", f != NULL);
    ok("query strip same file", f && f->len == 36);
}

static void test_find_miss(void)
{
    ok("miss -> NULL", tp_embed_find("/nope.html") == NULL);
    ok("NULL path -> NULL", tp_embed_find(NULL) == NULL);
}

static void test_ctypes_and_paths(void)
{
    const tp_embed_file *css = tp_embed_find("/css/a.css");
    const tp_embed_file *js = tp_embed_find("/js/a.js");
    const tp_embed_file *font = tp_embed_find("/fonts/a.woff2");

    ok("css find", css != NULL);
    ok("css ctype", css && strcmp(css->ctype, "text/css") == 0);
    ok("css len", css && css->len == 16);
    ok("css body", css && css->len >= 4 && memcmp(css->data, "body", 4) == 0);

    ok("js find", js != NULL);
    ok("js ctype", js && strcmp(js->ctype, "application/javascript") == 0);
    ok("js len", js && js->len == 16);

    ok("woff2 find", font != NULL);
    ok("woff2 ctype", font && strcmp(font->ctype, "font/woff2") == 0);
    ok("woff2 len", font && font->len == 2);
    ok("woff2 bytes", font && memcmp(font->data, "w2", 2) == 0);
}

int main(void)
{
    test_find_hit();
    test_find_query_strip();
    test_find_miss();
    test_ctypes_and_paths();

    if (failures)
    {
        printf("%d TEST(S) FAILED\n", failures);
        return 1;
    }
    puts("ALL TESTS PASSED");
    return 0;
}
