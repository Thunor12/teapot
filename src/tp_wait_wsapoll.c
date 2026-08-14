#include "teapot.h"

#if TEAPOT_WAIT == TEAPOT_WAIT_WSAPOLL
#ifndef _WIN32
#error "TEAPOT_WAIT_WSAPOLL requires Windows"
#else
#define TEAPOT_WAIT_IN 1
#define TEAPOT_WAIT_OUT 2
#define TP_WE(e) ((SHORT)(((e) & TEAPOT_WAIT_IN ? POLLIN : 0) | ((e) & TEAPOT_WAIT_OUT ? POLLOUT : 0)))

typedef struct { void *udata; int events; } tp_wait_event;
typedef struct { WSAPOLLFD *pfds; void **udata; int count; int cap; } tp_wait;

static int tp_wait_find(tp_wait *w, stb_teapot_socket_t fd)
{
    int i;
    for (i = 0; i < w->count; ++i)
        if (w->pfds[i].fd == fd) return i;
    return -1;
}

int tp_wait_create(tp_wait *w) { memset(w, 0, sizeof(*w)); return 0; }

int tp_wait_add(tp_wait *w, stb_teapot_socket_t fd, int events, void *udata)
{
    if (w->count == w->cap) {
        int cap = w->cap ? w->cap * 2 : 16;
        WSAPOLLFD *pfds = TP_REALLOC(w->pfds, (size_t)cap * sizeof(*pfds));
        void **ud;
        if (!pfds) return -1;
        w->pfds = pfds;
        ud = TP_REALLOC(w->udata, (size_t)cap * sizeof(*ud));
        if (!ud) return -1;
        w->udata = ud;
        w->cap = cap;
    }
    w->pfds[w->count].fd = fd;
    w->pfds[w->count].events = TP_WE(events);
    w->pfds[w->count].revents = 0;
    w->udata[w->count++] = udata;
    return 0;
}

int tp_wait_mod(tp_wait *w, stb_teapot_socket_t fd, int events, void *udata)
{
    int i = tp_wait_find(w, fd);
    if (i < 0) return -1;
    w->pfds[i].events = TP_WE(events);
    w->udata[i] = udata;
    return 0;
}

int tp_wait_del(tp_wait *w, stb_teapot_socket_t fd)
{
    int i = tp_wait_find(w, fd);
    if (i < 0) return -1;
    w->count--;
    if (i != w->count) { w->pfds[i] = w->pfds[w->count]; w->udata[i] = w->udata[w->count]; }
    return 0;
}

int tp_wait_wait(tp_wait *w, int timeout_ms, tp_wait_event *out, int max_out)
{
    int n = WSAPoll(w->pfds, (ULONG)w->count, timeout_ms), i, k = 0;
    if (n <= 0) return n;
    for (i = 0; i < w->count && k < max_out; ++i) {
        SHORT re = w->pfds[i].revents;
        int events = 0;
        if (re & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) events |= TEAPOT_WAIT_IN;
        if (re & POLLOUT) events |= TEAPOT_WAIT_OUT;
        if (!events) continue;
        out[k].udata = w->udata[i];
        out[k++].events = events;
    }
    return k;
}

void tp_wait_destroy(tp_wait *w)
{
    TP_FREE(w->pfds);
    TP_FREE(w->udata);
    memset(w, 0, sizeof(*w));
}
#define TP_WAIT_READY 1
#endif
#endif
