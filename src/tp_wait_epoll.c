#include "teapot.h"

#if TEAPOT_WAIT == TEAPOT_WAIT_EPOLL
#ifndef __linux__
#error "TEAPOT_WAIT_EPOLL requires Linux"
#else
#include <sys/epoll.h>

#define TEAPOT_WAIT_IN 1
#define TEAPOT_WAIT_OUT 2
#define TP_EE(e) (((e) & TEAPOT_WAIT_IN ? EPOLLIN : 0u) | ((e) & TEAPOT_WAIT_OUT ? EPOLLOUT : 0u))

typedef struct { void *udata; int events; } tp_wait_event;
typedef struct { int epfd; } tp_wait;

int tp_wait_create(tp_wait *w) { w->epfd = epoll_create1(0); return w->epfd < 0 ? -1 : 0; }

int tp_wait_add(tp_wait *w, stb_teapot_socket_t fd, int events, void *udata)
{
    struct epoll_event ev = {.events = TP_EE(events), .data.ptr = udata};
    return epoll_ctl(w->epfd, EPOLL_CTL_ADD, fd, &ev) == 0 ? 0 : -1;
}

int tp_wait_mod(tp_wait *w, stb_teapot_socket_t fd, int events, void *udata)
{
    struct epoll_event ev = {.events = TP_EE(events), .data.ptr = udata};
    return epoll_ctl(w->epfd, EPOLL_CTL_MOD, fd, &ev) == 0 ? 0 : -1;
}

int tp_wait_del(tp_wait *w, stb_teapot_socket_t fd)
{
    return epoll_ctl(w->epfd, EPOLL_CTL_DEL, fd, NULL) == 0 ? 0 : -1;
}

int tp_wait_wait(tp_wait *w, int timeout_ms, tp_wait_event *out, int max_out)
{
    struct epoll_event buf[64];
    int lim = max_out < 64 ? max_out : 64, n, i, k = 0;
    do n = epoll_wait(w->epfd, buf, lim, timeout_ms); while (n < 0 && errno == EINTR);
    if (n <= 0) return n;
    for (i = 0; i < n; ++i) {
        uint32_t re = buf[i].events;
        int events = 0;
        if (re & (EPOLLIN | EPOLLHUP | EPOLLERR)) events |= TEAPOT_WAIT_IN;
        if (re & EPOLLOUT) events |= TEAPOT_WAIT_OUT;
        if (!events) continue;
        out[k].udata = buf[i].data.ptr;
        out[k++].events = events;
    }
    return k;
}

void tp_wait_destroy(tp_wait *w)
{
    if (w->epfd >= 0) close(w->epfd);
    memset(w, 0, sizeof(*w));
    w->epfd = -1;
}
#define TP_WAIT_READY 1
#endif
#endif
