#include "teapot.h"

#include <limits.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#endif

#ifndef TEAPOT_CONN_BUF
#define TEAPOT_CONN_BUF 8192
#endif

#ifndef TEAPOT_MAX_CONNS
#define TEAPOT_MAX_CONNS 128
#endif

    typedef enum
    {
        TEAPOT_IO_NEED_READ = 1,
        TEAPOT_IO_NEED_WRITE = 2,
        TEAPOT_IO_DONE = 0,
        TEAPOT_IO_ERROR = -1,
    } teapot_io;

    typedef enum
    {
        TEAPOT_CONN_READ_HEAD,
        TEAPOT_CONN_READ_BODY,
        TEAPOT_CONN_WRITE_RESP,
        TEAPOT_CONN_DONE,
    } teapot_conn_phase;

    typedef struct teapot_conn
    {
        teapot_server *server;
        stb_teapot_socket_t fd;
        teapot_conn_phase phase;
        char in[TEAPOT_CONN_BUF];
        size_t in_len;
        size_t header_end; /* index of '\r' of the blank line; 0 until seen */
        size_t body_need;
        size_t body_got;
        teapot_request req;
        teapot_response res;
        tp_string_builder out;
        size_t out_sent;
        uint64_t deadline_ms; /* recv at init; reset when entering WRITE_RESP */
        int failed;
        int slot_used;
    } teapot_conn;

    static size_t teapot_find_header_end(const char *buf, size_t n)
    {
        if (n < 4)
            return (size_t)-1;
        for (size_t i = 0; i + 3 < n; ++i)
        {
            if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n')
                return i;
        }
        return (size_t)-1;
    }

    static void teapot_conn_arm_send_deadline(teapot_conn *c)
    {
#if TEAPOT_SEND_TIMEOUT_MS > 0
        c->deadline_ms = tp_now_ms() + (uint64_t)TEAPOT_SEND_TIMEOUT_MS;
#else
        c->deadline_ms = 0;
#endif
    }

    static teapot_io teapot_conn_begin_400(teapot_conn *c)
    {
        teapot_response_free(&c->res);
        teapot_response_init(&c->res, TEAPOT_HTTP_BAD_REQUEST);
        tp_sb_appendf(&c->res.body, "400 Bad Request\n");
        tp_sb_free(c->out);
        c->out = (tp_string_builder){0};
        c->out_sent = 0;
        if (teapot_format_response(&c->out, &c->res) != 0)
        {
            c->failed = 1;
            c->phase = TEAPOT_CONN_DONE;
            return TEAPOT_IO_ERROR;
        }
        c->failed = 1;
        c->phase = TEAPOT_CONN_WRITE_RESP;
        teapot_conn_arm_send_deadline(c);
        return TEAPOT_IO_NEED_WRITE;
    }

    static teapot_io teapot_conn_dispatch(teapot_conn *c)
    {
        c->req.user = c->server->user;
        teapot_handler handler = teapot_find_handler(c->server, &c->req);
        teapot_response_free(&c->res);
        teapot_response_init(&c->res, TEAPOT_HTTP_OK);
        if (handler)
            c->res = handler(&c->req);
        else
        {
            c->res.status = TEAPOT_HTTP_NOT_FOUND;
            tp_sb_appendf(&c->res.body, "404 Not Found\n");
        }

        tp_sb_free(c->out);
        c->out = (tp_string_builder){0};
        c->out_sent = 0;
        if (teapot_format_response(&c->out, &c->res) != 0)
        {
            c->failed = 1;
            c->phase = TEAPOT_CONN_DONE;
            return TEAPOT_IO_ERROR;
        }
        c->failed = 0;
        c->phase = TEAPOT_CONN_WRITE_RESP;
        teapot_conn_arm_send_deadline(c);
        return TEAPOT_IO_NEED_WRITE;
    }

    void teapot_conn_init(teapot_conn *c, teapot_server *server, stb_teapot_socket_t fd)
    {
        memset(c, 0, sizeof(*c));
        c->server = server;
        c->fd = fd;
        c->phase = TEAPOT_CONN_READ_HEAD;
#if TEAPOT_RECV_TIMEOUT_MS > 0
        c->deadline_ms = tp_now_ms() + (uint64_t)TEAPOT_RECV_TIMEOUT_MS;
#else
        c->deadline_ms = 0;
#endif
    }

    void teapot_conn_free(teapot_conn *c)
    {
        if (!c)
            return;
        free_request(&c->req);
        c->req.path = (tp_string_builder){0};
        c->req.body = (tp_string_builder){0};
        teapot_response_free(&c->res);
        tp_sb_free(c->out);
        c->out = (tp_string_builder){0};
    }

    static teapot_io teapot_conn_read_head(teapot_conn *c)
    {
        if (c->in_len == (size_t)TEAPOT_CONN_BUF)
            return teapot_conn_begin_400(c);

        int space = (int)((size_t)TEAPOT_CONN_BUF - c->in_len);
        int n = (int)recv(c->fd, c->in + c->in_len, (size_t)space, 0);
        if (n < 0)
        {
            if (teapot_would_block())
                return TEAPOT_IO_NEED_READ;
            c->failed = 1;
            c->phase = TEAPOT_CONN_DONE;
            return TEAPOT_IO_ERROR;
        }
        if (n == 0)
            return teapot_conn_begin_400(c);

        c->in_len += (size_t)n;
        size_t hend = teapot_find_header_end(c->in, c->in_len);
        if (hend == (size_t)-1)
        {
            if (c->in_len == (size_t)TEAPOT_CONN_BUF)
                return teapot_conn_begin_400(c);
            return TEAPOT_IO_NEED_READ;
        }

        c->header_end = hend;
        free_request(&c->req);
        c->req = (teapot_request){0};
        size_t content_length = 0;
        if (parse_request(c->in, c->in_len, &c->req, &content_length) < 0)
            return teapot_conn_begin_400(c);

        c->body_need = content_length;
        c->body_got = c->req.body_length;
        if (c->body_need > (size_t)TEAPOT_MAX_BODY_SIZE)
            return teapot_conn_begin_400(c);

        if (c->body_got >= c->body_need)
        {
            if (c->req.body.count == c->req.body_length)
                tp_sb_append_null(&c->req.body);
            return teapot_conn_dispatch(c);
        }

        c->phase = TEAPOT_CONN_READ_BODY;
        return TEAPOT_IO_NEED_READ;
    }

    static teapot_io teapot_conn_read_body(teapot_conn *c)
    {
        if (c->body_need > (size_t)TEAPOT_MAX_BODY_SIZE)
            return teapot_conn_begin_400(c);

        char bounce[4096];
        size_t remaining = c->body_need - c->body_got;
        size_t to_read = remaining > sizeof(bounce) ? sizeof(bounce) : remaining;
        int n = (int)recv(c->fd, bounce, to_read, 0);
        if (n < 0)
        {
            if (teapot_would_block())
                return TEAPOT_IO_NEED_READ;
            c->failed = 1;
            c->phase = TEAPOT_CONN_DONE;
            return TEAPOT_IO_ERROR;
        }
        if (n == 0)
            return teapot_conn_begin_400(c);

        c->req.body.count = c->req.body_length;
        tp_sb_append_buf(&c->req.body, bounce, (size_t)n);
        c->req.body_length += (size_t)n;
        c->body_got = c->req.body_length;

        if (c->body_got < c->body_need)
            return TEAPOT_IO_NEED_READ;

        tp_sb_append_null(&c->req.body);
        return teapot_conn_dispatch(c);
    }

    static teapot_io teapot_conn_write_resp(teapot_conn *c)
    {
        if (c->out_sent >= c->out.count)
        {
            c->phase = TEAPOT_CONN_DONE;
            return TEAPOT_IO_DONE;
        }

        size_t remaining = c->out.count - c->out_sent;
        int chunk = remaining > (size_t)INT_MAX ? INT_MAX : (int)remaining;
        int n = teapot_write(c->fd, c->out.items + c->out_sent, chunk);
        if (n < 0)
        {
            if (teapot_would_block())
                return TEAPOT_IO_NEED_WRITE;
            c->failed = 1;
            c->phase = TEAPOT_CONN_DONE;
            return TEAPOT_IO_ERROR;
        }
        if (n == 0)
        {
            c->failed = 1;
            c->phase = TEAPOT_CONN_DONE;
            return TEAPOT_IO_ERROR;
        }
        c->out_sent += (size_t)n;
        if (c->out_sent >= c->out.count)
        {
            c->phase = TEAPOT_CONN_DONE;
            return TEAPOT_IO_DONE;
        }
        return TEAPOT_IO_NEED_WRITE;
    }

    teapot_io teapot_conn_step(teapot_conn *c)
    {
        if (!c || !c->server || !teapot_socket_ok(c->fd))
            return TEAPOT_IO_ERROR;

        switch (c->phase)
        {
        case TEAPOT_CONN_READ_HEAD:
            return teapot_conn_read_head(c);
        case TEAPOT_CONN_READ_BODY:
            return teapot_conn_read_body(c);
        case TEAPOT_CONN_WRITE_RESP:
            return teapot_conn_write_resp(c);
        case TEAPOT_CONN_DONE:
            return c->failed ? TEAPOT_IO_ERROR : TEAPOT_IO_DONE;
        default:
            c->failed = 1;
            return TEAPOT_IO_ERROR;
        }
    }

    int teapot_serve_client(teapot_server *server, stb_teapot_socket_t client)
    {
        if (!server || !teapot_socket_ok(client))
            return -1;
        if (teapot_set_nonblock(client) != 0)
            return -1;

        teapot_conn c;
        teapot_conn_init(&c, server, client);
        int rc = -1;

        for (;;)
        {
            teapot_io io = teapot_conn_step(&c);
            if (io == TEAPOT_IO_DONE)
            {
                rc = c.failed ? -1 : 0;
                break;
            }
            if (io == TEAPOT_IO_ERROR)
            {
                rc = -1;
                break;
            }

            int timeout_ms = 250;
#if TEAPOT_RECV_TIMEOUT_MS > 0 || TEAPOT_SEND_TIMEOUT_MS > 0
            if (c.deadline_ms != 0 &&
                (c.phase == TEAPOT_CONN_READ_HEAD || c.phase == TEAPOT_CONN_READ_BODY ||
                 c.phase == TEAPOT_CONN_WRITE_RESP))
            {
                uint64_t now = tp_now_ms();
                if (now >= c.deadline_ms)
                {
                    teapot_conn_free(&c);
                    return -1;
                }
                uint64_t rem = c.deadline_ms - now;
                if (rem < (uint64_t)timeout_ms)
                    timeout_ms = (int)rem;
                if (timeout_ms < 1)
                    timeout_ms = 1;
            }
#endif

#ifdef _WIN32
            WSAPOLLFD pfd;
            pfd.fd = client;
            pfd.events = (io == TEAPOT_IO_NEED_READ) ? POLLIN : POLLOUT;
            pfd.revents = 0;
            int pr = WSAPoll(&pfd, 1, timeout_ms);
#else
            struct pollfd pfd;
            pfd.fd = client;
            pfd.events = (io == TEAPOT_IO_NEED_READ) ? POLLIN : POLLOUT;
            pfd.revents = 0;
            int pr = poll(&pfd, 1, timeout_ms);
#endif
            if (server->stop)
            {
                teapot_conn_free(&c);
                return -1;
            }
            if (pr < 0)
            {
#ifdef _WIN32
                teapot_conn_free(&c);
                return -1;
#else
                if (errno == EINTR)
                    continue;
                teapot_conn_free(&c);
                return -1;
#endif
            }
            if (pr == 0)
            {
#if TEAPOT_RECV_TIMEOUT_MS > 0 || TEAPOT_SEND_TIMEOUT_MS > 0
                if (c.deadline_ms != 0 &&
                    (c.phase == TEAPOT_CONN_READ_HEAD || c.phase == TEAPOT_CONN_READ_BODY ||
                     c.phase == TEAPOT_CONN_WRITE_RESP) &&
                    tp_now_ms() >= c.deadline_ms)
                {
                    teapot_conn_free(&c);
                    return -1;
                }
#endif
                continue;
            }
        }

        teapot_conn_free(&c);
        return rc;
    }
