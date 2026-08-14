#include "teapot.h"

#ifdef TP_WAIT_READY

typedef struct
{
    teapot_server *server;
    tp_wait *w;
    stb_teapot_socket_t listen_sock;
    teapot_conn *slab;
    int max;
    int listen_armed;
} tp_run_ctx;

static void tp_run_disarm(tp_run_ctx *ctx)
{
    if (!ctx->listen_armed)
        return;
    tp_wait_del(ctx->w, ctx->listen_sock);
    ctx->listen_armed = 0;
}

static void tp_run_drop(tp_run_ctx *ctx, teapot_conn *c)
{
    tp_wait_del(ctx->w, c->fd);
    teapot_conn_free(c);
    teapot_close(c->fd);
    c->slot_used = 0;
    if (!ctx->listen_armed &&
        tp_wait_add(ctx->w, ctx->listen_sock, TEAPOT_WAIT_IN, NULL) == 0)
        ctx->listen_armed = 1;
}

static int tp_run_walk_reads(tp_run_ctx *ctx, int drop)
{
    int ms = 250, i;
    uint64_t now = tp_now_ms();
    for (i = 0; i < ctx->max; ++i)
    {
        teapot_conn *c = &ctx->slab[i];
        uint64_t rem;
        if (!c->slot_used || !c->deadline_ms)
            continue;
        if (c->phase != TEAPOT_CONN_READ_HEAD && c->phase != TEAPOT_CONN_READ_BODY)
            continue;
        if (now >= c->deadline_ms)
        {
            if (drop)
                tp_run_drop(ctx, c);
            else
                return 0;
            continue;
        }
        rem = c->deadline_ms - now;
        if (rem < (uint64_t)ms)
            ms = (int)rem;
        if (ms < 1)
            ms = 1;
    }
    return ms;
}

static void tp_run_accept(tp_run_ctx *ctx)
{
    for (;;)
    {
        int slot;
        stb_teapot_socket_t client;
        teapot_conn *c;
        if (ctx->server->stop)
            return;
        for (slot = 0; slot < ctx->max; ++slot)
            if (!ctx->slab[slot].slot_used)
                break;
        if (slot == ctx->max)
        {
            tp_run_disarm(ctx);
            return;
        }
        client = teapot_listener_accept(ctx->listen_sock);
        if (!teapot_socket_ok(client))
            return;
        c = &ctx->slab[slot];
        if (teapot_set_nonblock(client) != 0)
        {
            teapot_close(client);
            tp_run_disarm(ctx);
            return;
        }
        teapot_conn_init(c, ctx->server, client);
        c->slot_used = 1;
        if (tp_wait_add(ctx->w, client, TEAPOT_WAIT_IN, c) != 0)
        {
            teapot_conn_free(c);
            c->slot_used = 0;
            teapot_close(client);
            tp_run_disarm(ctx);
            return;
        }
    }
}

int teapot_run(teapot_server *server)
{
    stb_teapot_socket_t listen_sock;
    tp_wait w;
    teapot_conn *slab;
    tp_run_ctx ctx;
    int max, i;
    tp_wait_event ev[64];

    if (!server)
        return 1;
    if (teapot_listener_open(server, &listen_sock) < 0)
        return 1;
    if (teapot_set_nonblock(listen_sock) != 0 || tp_wait_create(&w) != 0)
    {
        teapot_listener_close(listen_sock);
        return 1;
    }
    max = server->max_conns > 0 ? server->max_conns : TEAPOT_MAX_CONNS;
    slab = TP_REALLOC(NULL, (size_t)max * sizeof(*slab));
    if (!slab || tp_wait_add(&w, listen_sock, TEAPOT_WAIT_IN, NULL) != 0)
    {
        TP_FREE(slab);
        tp_wait_destroy(&w);
        teapot_listener_close(listen_sock);
        return 1;
    }
    memset(slab, 0, (size_t)max * sizeof(*slab));
    ctx = (tp_run_ctx){server, &w, listen_sock, slab, max, 1};
    while (!server->stop)
    {
        int n = tp_wait_wait(&w, tp_run_walk_reads(&ctx, 0), ev, 64);
        tp_run_walk_reads(&ctx, 1);
        if (server->stop)
            break;
        if (n < 0)
            continue;
        for (i = 0; i < n; ++i)
        {
            teapot_conn *c = ev[i].udata;
            teapot_io io;
            if (!c)
            {
                tp_run_accept(&ctx);
                continue;
            }
            if (!c->slot_used)
                continue;
            io = teapot_conn_step(c);
            if (io == TEAPOT_IO_NEED_READ)
                tp_wait_mod(&w, c->fd, TEAPOT_WAIT_IN, c);
            else if (io == TEAPOT_IO_NEED_WRITE)
                tp_wait_mod(&w, c->fd, TEAPOT_WAIT_OUT, c);
            else
                tp_run_drop(&ctx, c);
        }
    }
    for (i = 0; i < max; ++i)
    {
        if (!slab[i].slot_used)
            continue;
        tp_wait_del(&w, slab[i].fd);
        teapot_conn_free(&slab[i]);
        teapot_close(slab[i].fd);
    }
    TP_FREE(slab);
    tp_wait_destroy(&w);
    teapot_listener_close(listen_sock);
    return 0;
}

#endif
