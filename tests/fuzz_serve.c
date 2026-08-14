#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
int main(void)
{
    puts("fuzz_serve is POSIX-only");
    return 0;
}
#else
#include <sys/socket.h>
#include <unistd.h>

static int handler_called;
static size_t observed_body_length;
static size_t observed_content_length;

static teapot_response fuzz_handler(const teapot_request *req)
{
    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);

    handler_called = 1;
    observed_body_length = req->body_length;
    observed_content_length = 0;

    const tp_string_builder *val = tp_headers_get(&req->headers, "Content-Length");
    if (val != NULL && val->items != NULL && val->items[0] != '\0')
    {
        char *end = NULL;
        unsigned long n = strtoul(val->items, &end, 10);
        if (end != val->items)
            observed_content_length = (size_t)n;
    }

    teapot_response_write(&resp, "ok", 2);
    return resp;
}

static teapot_route fuzz_routes[] = {
    {TEAPOT_GET, "/", fuzz_handler, 0},
    {TEAPOT_POST, "/echo/", fuzz_handler, 1},
};

static teapot_server fuzz_server = {
    .port = 0,
    .routes = fuzz_routes,
    .route_count = 2,
};

static int is_tchar(unsigned char c)
{
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
        return 1;
    switch (c)
    {
    case '!':
    case '#':
    case '$':
    case '%':
    case '&':
    case '\'':
    case '*':
    case '+':
    case '-':
    case '.':
    case '^':
    case '_':
    case '`':
    case '|':
    case '~':
        return 1;
    default:
        return 0;
    }
}

static int name_is(const char *s, size_t n, const char *lit)
{
    size_t ln = strlen(lit);
    if (n != ln)
        return 0;
    for (size_t i = 0; i < n; i++)
    {
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)lit[i]))
            return 0;
    }
    return 1;
}

static int method_ok(const char *s, size_t n)
{
    return (n == 3 && memcmp(s, "GET", 3) == 0) || (n == 4 && memcmp(s, "POST", 4) == 0) ||
           (n == 3 && memcmp(s, "PUT", 3) == 0) || (n == 6 && memcmp(s, "DELETE", 6) == 0);
}

static const char *find_crlf(const char *p, const char *end)
{
    for (const char *q = p; q + 1 < end; q++)
    {
        if (q[0] == '\r' && q[1] == '\n')
            return q;
    }
    return NULL;
}

static int rfc_lite_is_valid(const uint8_t *data, size_t size)
{
    const char *p = (const char *)data;
    const char *end = p + size;
    const char *crlf = find_crlf(p, end);
    if (crlf == NULL)
        return 0;

    size_t line_n = (size_t)(crlf - p);
    if (line_n == 0)
        return 0;

    size_t i = 0;
    while (i < line_n && p[i] != ' ')
        i++;
    if (i == 0 || i >= line_n)
        return 0;
    if (!method_ok(p, i))
        return 0;

    size_t j = i;
    while (j < line_n && p[j] == ' ')
        j++;
    if (j == i)
        return 0;

    size_t path0 = j;
    while (j < line_n && p[j] != ' ')
    {
        if (p[j] == '\r' || p[j] == '\n')
            return 0;
        j++;
    }
    if (j == path0 || j >= line_n)
        return 0;

    size_t k = j;
    while (k < line_n && p[k] == ' ')
        k++;
    if (k == j)
        return 0;
    size_t ver_n = line_n - k;
    if (ver_n != 8)
        return 0;
    if (memcmp(p + k, "HTTP/1.0", 8) != 0 && memcmp(p + k, "HTTP/1.1", 8) != 0)
        return 0;

    const char *hdr = crlf + 2;
    int saw_cl = 0;
    unsigned long cl = 0;
    const char *headers_end = NULL;

    while (hdr < end)
    {
        if (hdr + 1 < end && hdr[0] == '\r' && hdr[1] == '\n')
        {
            headers_end = hdr + 2;
            break;
        }
        const char *eol = find_crlf(hdr, end);
        if (eol == NULL)
            return 0;
        size_t linelen = (size_t)(eol - hdr);
        const char *colon = (const char *)memchr(hdr, ':', linelen);
        if (colon == NULL)
            return 0;
        size_t namelen = (size_t)(colon - hdr);
        if (namelen == 0 || namelen > (size_t)TP_MAX_HEADER_NAME_LEN)
            return 0;
        for (size_t n = 0; n < namelen; n++)
        {
            if (!is_tchar((unsigned char)hdr[n]))
                return 0;
        }

        const char *val = colon + 1;
        const char *val_end = eol;
        while (val < val_end && (*val == ' ' || *val == '\t'))
            val++;
        while (val_end > val && (val_end[-1] == ' ' || val_end[-1] == '\t'))
            val_end--;
        size_t vlen = (size_t)(val_end - val);
        if (vlen > (size_t)TP_MAX_HEADER_VALUE_LEN)
            return 0;

        if (name_is(hdr, namelen, "Transfer-Encoding"))
            return 0;
        if (name_is(hdr, namelen, "Content-Length"))
        {
            if (saw_cl)
                return 0;
            saw_cl = 1;
            if (vlen == 0 || vlen >= 32)
                return 0;
            char tmp[32];
            memcpy(tmp, val, vlen);
            tmp[vlen] = '\0';
            char *endp = NULL;
            cl = strtoul(tmp, &endp, 10);
            if (endp == tmp)
                return 0;
            while (*endp != '\0' && isspace((unsigned char)*endp))
                ++endp;
            if (*endp != '\0' || cl > (unsigned long)TEAPOT_MAX_BODY_SIZE)
                return 0;
        }
        hdr = eol + 2;
    }
    if (headers_end == NULL)
        return 0;

    if ((size_t)(headers_end - p) > (size_t)TEAPOT_CONN_BUF)
        return 0;

    size_t body_n = (size_t)(end - headers_end);
    if (!saw_cl)
        return body_n == 0;
    return body_n == (size_t)cl;
}

static int grammar_oracle_enabled(void)
{
    const char *e = getenv("TEAPOT_FUZZ_GRAMMAR");
    return e != NULL && e[0] != '\0';
}

static int write_all_raw(int fd, const uint8_t *buf, size_t len)
{
    size_t total = 0;
    while (total < len)
    {
        ssize_t n = write(fd, buf + total, len - total);
        if (n <= 0)
            return -1;
        total += (size_t)n;
    }
    return 0;
}

static int response_status_400(const char *buf, size_t n)
{
    if (n < 12)
        return 0;
    return memcmp(buf, "HTTP/1.1 400", 12) == 0;
}

int fuzz_one(const uint8_t *data, size_t size)
{
    handler_called = 0;
    observed_body_length = 0;
    observed_content_length = 0;

    if (size == 0)
        return 0;

    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0)
        return 0;

    if (write_all_raw(fds[1], data, size) != 0)
    {
        teapot_close(fds[0]);
        teapot_close(fds[1]);
        return 0;
    }
    shutdown(fds[1], SHUT_WR);

    int rc = teapot_serve_client(&fuzz_server, fds[0]);

    int still_open = fcntl(fds[0], F_GETFD) >= 0;
    if (!still_open)
    {
        teapot_close(fds[1]);
        abort();
    }

    /* Serve must not close the fd; SHUT_WR lets the peer drain without hanging. */
    shutdown(fds[0], SHUT_WR);

    char resp[2048];
    size_t resp_n = 0;
    while (resp_n + 1 < sizeof(resp))
    {
        ssize_t n = read(fds[1], resp + resp_n, sizeof(resp) - resp_n - 1);
        if (n <= 0)
            break;
        resp_n += (size_t)n;
    }
    resp[resp_n] = '\0';

    if (handler_called)
    {
        int body_too_big = observed_body_length > observed_content_length || observed_body_length > size;
        if (body_too_big)
        {
            teapot_close(fds[0]);
            teapot_close(fds[1]);
            abort();
        }
    }

    int grammar_fail = 0;
    if (grammar_oracle_enabled() && rfc_lite_is_valid(data, size))
    {
        if (rc < 0 || response_status_400(resp, resp_n))
            grammar_fail = 1;
    }

    teapot_close(fds[0]);
    teapot_close(fds[1]);
    if (grammar_fail)
        abort();
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    return fuzz_one(data, size);
}

#endif
