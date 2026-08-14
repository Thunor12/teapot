#include "teapot.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

    static int tp_stricmp(const char *a, const char *b)
    {
        if (a == b)
        {
            return 0;
        }

        if (!a)
        {
            return b ? -1 : 0;
        }

        if (!b)
        {
            return 1;
        }

        while (*a && *b)
        {
            int ca = tolower((unsigned char)*a);
            int cb = tolower((unsigned char)*b);

            if (ca != cb)
            {
                return ca - cb;
            }

            ++a;
            ++b;
        }
        return tolower((unsigned char)*a) - tolower((unsigned char)*b);
    }

    /* helper: find header line by name (returns NULL if not found) */
    static const tp_header_line *tp_headers_find(const tp_headers *h, const char *name)
    {
        if (h == NULL || name == NULL)
            return NULL;
        for (size_t i = 0; i < h->count; ++i)
        {
            const char *hn = h->items[i].name.items ? h->items[i].name.items : "";
            if (tp_stricmp(hn, name) == 0)
                return &h->items[i];
        }
        return NULL;
    }

    typedef struct
    {
        const char *p;
        size_t n;
    } tp_span;

    static tp_span tp_span_trim(const char *s, size_t len)
    {
        while (len > 0 && isspace((unsigned char)*s))
        {
            ++s;
            --len;
        }
        while (len > 0 && isspace((unsigned char)s[len - 1]))
        {
            --len;
        }
        return (tp_span){s, len};
    }

    /* HTTP/1.1 subset: no obs-fold. Oversize names/values are rejected. */
    static int tp_parse_and_append_header_line(tp_headers *headers_parsed, const char *line, size_t linelen)
    {
        if (!headers_parsed || !line || linelen == 0)
        {
            return 0;
        }

        const char *colon = (const char *)memchr(line, ':', linelen);
        if (!colon)
        {
            return 0;
        }

        tp_span name = tp_span_trim(line, (size_t)(colon - line));
        tp_span value = tp_span_trim(colon + 1, (size_t)((line + linelen) - (colon + 1)));

        if (name.n == 0)
        {
            return 0;
        }
        if (name.n > (size_t)TP_MAX_HEADER_NAME_LEN || value.n > (size_t)TP_MAX_HEADER_VALUE_LEN)
        {
            return 0;
        }

        tp_header_line header_line = {0};
        tp_sb_append_buf(&header_line.name, name.p, name.n);
        tp_sb_append_null(&header_line.name);
        if (value.n > 0)
        {
            tp_sb_append_buf(&header_line.value, value.p, value.n);
            tp_sb_append_null(&header_line.value);
        }
        tp_da_append(headers_parsed, header_line);
        return 1;
    }

    static const char *tp_find_crlf(const char *buf, size_t n)
    {
        const char *end = buf + n;
        while (buf < end)
        {
            const char *p = (const char *)memchr(buf, '\r', (size_t)(end - buf));
            if (!p || p + 1 >= end)
                return NULL;
            if (p[1] == '\n')
                return p;
            buf = p + 1;
        }
        return NULL;
    }

    /* 1 = blank line ended the block, 0 = buffer ended, -1 = bad line. */
    static int tp_parse_header_block(tp_headers *h, const char *raw, size_t n, size_t *out_consumed)
    {
        size_t i = 0;
        while (i < n)
        {
            if (i + 1 < n && raw[i] == '\r' && raw[i + 1] == '\n')
            {
                *out_consumed = i + 2;
                return 1;
            }
            const char *eol = tp_find_crlf(raw + i, n - i);
            if (!eol)
                return -1;
            size_t linelen = (size_t)(eol - (raw + i));
            if (tp_parse_and_append_header_line(h, raw + i, linelen) != 1)
                return -1;
            i = (size_t)(eol - raw) + 2;
        }
        *out_consumed = i;
        return 0;
    }

    int tp_extract_header_keyval(tp_headers *headers_parsed, const char *raw_header, size_t header_size)
    {
        if (headers_parsed == NULL || raw_header == NULL)
            return -1;
        if (header_size == 0)
            return 0;

#ifdef TP_MAX_HEADER_TOTAL
        if (header_size > (size_t)TP_MAX_HEADER_TOTAL)
            header_size = (size_t)TP_MAX_HEADER_TOTAL;
#endif

        size_t consumed = 0;
        if (tp_parse_header_block(headers_parsed, raw_header, header_size, &consumed) < 0)
            return -1;
        (void)consumed;
        return 0;
    }

    const tp_string_builder *tp_headers_get(const tp_headers *h, const char *name)
    {
        const tp_header_line *hl = tp_headers_find(h, name);
        return hl ? &hl->value : NULL;
    }

    tp_header_result tp_headers_check(const tp_headers *h, const char *name, const char *expected_value, tp_header_line *o_header_line)
    {
        const tp_header_line *hl = tp_headers_find(h, name);
        if (!hl)
        {
            return TP_HEADER_NOT_FOUND;
        }
        if (o_header_line)
        {
            *o_header_line = *hl;
        }
        if (expected_value == NULL)
        {
            return TP_HEADER_FOUND;
        }
        const char *val = hl->value.items ? hl->value.items : "";
        return (strcmp(val, expected_value) == 0) ? TP_HEADER_MATCH : TP_HEADER_FOUND;
    }

    static const char *tp_skip_spaces(const char *p, const char *end)
    {
        while (p < end && *p == ' ')
            ++p;
        return p;
    }

    static teapot_method parse_method(const char *s, size_t n)
    {
        if (n == 3 && memcmp(s, "GET", 3) == 0)
            return TEAPOT_GET;
        if (n == 4 && memcmp(s, "POST", 4) == 0)
            return TEAPOT_POST;
        if (n == 3 && memcmp(s, "PUT", 3) == 0)
            return TEAPOT_PUT;
        if (n == 6 && memcmp(s, "DELETE", 6) == 0)
            return TEAPOT_DELETE;
        return TEAPOT_UNKNOWN;
    }

    static int teapot_content_length_from_headers(const tp_headers *h, size_t *out_length)
    {
        *out_length = 0;
        int saw_cl = 0;
        unsigned long cl = 0;
        for (size_t i = 0; i < h->count; ++i)
        {
            const char *name = h->items[i].name.items ? h->items[i].name.items : "";
            if (tp_stricmp(name, "Transfer-Encoding") == 0)
                return -1;
            if (tp_stricmp(name, "Content-Length") != 0)
                continue;
            const char *val = h->items[i].value.items ? h->items[i].value.items : "";
            char *end = NULL;
            unsigned long n = strtoul(val, &end, 10);
            if (end == val)
                return -1;
            while (*end != '\0' && isspace((unsigned char)*end))
                ++end;
            if (*end != '\0' || n > (unsigned long)TEAPOT_MAX_BODY_SIZE)
                return -1;
            if (saw_cl && n != cl)
                return -1;
            saw_cl = 1;
            cl = n;
        }
        if (saw_cl)
            *out_length = (size_t)cl;
        return 0;
    }

    static void free_request(teapot_request *req)
    {
        tp_sb_free(req->path);
        tp_sb_free(req->body);
        tp_headers_free(&req->headers);
    }

    static int parse_request(char *buffer, size_t size, teapot_request *req, size_t *out_content_length)
    {
        if (buffer == NULL || size == 0 || req == NULL || out_content_length == NULL)
            return -1;

        req->path = (tp_string_builder){0};
        req->body = (tp_string_builder){0};
        req->headers = (tp_headers){0};
        req->body_length = 0;
        *out_content_length = 0;

        const char *line_end = tp_find_crlf(buffer, size);
        if (!line_end)
            return -1;

        size_t line_n = (size_t)(line_end - buffer);
        const char *sp1 = (const char *)memchr(buffer, ' ', line_n);
        if (!sp1)
            return -1;
        teapot_method method = parse_method(buffer, (size_t)(sp1 - buffer));
        if (method == TEAPOT_UNKNOWN)
            return -1;

        const char *path0 = tp_skip_spaces(sp1 + 1, line_end);
        if (path0 >= line_end)
            return -1;
        const char *sp2 = (const char *)memchr(path0, ' ', (size_t)(line_end - path0));
        if (!sp2)
            return -1;
        size_t path_n = (size_t)(sp2 - path0);
        if (path_n == 0)
            return -1;

        size_t line_end_off = (size_t)(line_end - buffer);
        if (line_end_off + 2 > size)
            return -1;
        size_t i = line_end_off + 2;
        size_t consumed = 0;
        if (tp_parse_header_block(&req->headers, buffer + i, size - i, &consumed) != 1)
            return -1;
        i += consumed;

        size_t content_length = 0;
        if (teapot_content_length_from_headers(&req->headers, &content_length) != 0)
            return -1;

        const char *body_start = buffer + i;
        size_t body_available = size > i ? size - i : 0;
        if (body_available > content_length)
            return -1;
        size_t to_append = content_length < body_available ? content_length : body_available;

        req->method = method;
        tp_sb_append_buf(&req->path, path0, path_n);
        tp_sb_append_null(&req->path);
        tp_sb_append_buf(&req->body, body_start, to_append);
        tp_sb_append_null(&req->body);
        req->body_length = to_append;
        *out_content_length = content_length;
        return 0;
    }
