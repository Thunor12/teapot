#include "teapot.h"

#if TEAPOT_WAIT == TEAPOT_WAIT_KQUEUE
#if !(defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__))
#error "TEAPOT_WAIT_KQUEUE requires Apple or BSD"
#else
#include <errno.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

#define TEAPOT_WAIT_IN 1
#define TEAPOT_WAIT_OUT 2

typedef struct { void *udata; int events; } tp_wait_event;
typedef struct { int kq; } tp_wait;

static int tp_kq_set(int kq, stb_teapot_socket_t fd, int events, void *udata)
{
    struct kevent ch[2];
    int n = 0;
    if (events & TEAPOT_WAIT_IN)
        EV_SET(&ch[n++], (uintptr_t)fd, EVFILT_READ, EV_ADD, 0, 0, udata);
    if (events & TEAPOT_WAIT_OUT)
        EV_SET(&ch[n++], (uintptr_t)fd, EVFILT_WRITE, EV_ADD, 0, 0, udata);
    return n && kevent(kq, ch, n, NULL, 0, NULL) == 0 ? 0 : -1;
}

static void tp_kq_clr(int kq, stb_teapot_socket_t fd)
{
    struct kevent ch;
    EV_SET(&ch, (uintptr_t)fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    (void)kevent(kq, &ch, 1, NULL, 0, NULL);
    EV_SET(&ch, (uintptr_t)fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    (void)kevent(kq, &ch, 1, NULL, 0, NULL);
}

int tp_wait_create(tp_wait *w) { w->kq = kqueue(); return w->kq < 0 ? -1 : 0; }
int tp_wait_add(tp_wait *w, stb_teapot_socket_t fd, int events, void *udata)
{ return tp_kq_set(w->kq, fd, events, udata); }
int tp_wait_mod(tp_wait *w, stb_teapot_socket_t fd, int events, void *udata)
{ tp_kq_clr(w->kq, fd); return tp_kq_set(w->kq, fd, events, udata); }
int tp_wait_del(tp_wait *w, stb_teapot_socket_t fd) { tp_kq_clr(w->kq, fd); return 0; }

int tp_wait_wait(tp_wait *w, int timeout_ms, tp_wait_event *out, int max_out)
{
    struct kevent buf[64];
    struct timespec ts, *tsp = NULL;
    int lim = max_out < 64 ? max_out : 64, n, i, k = 0;
    if (timeout_ms >= 0) {
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;
        tsp = &ts;
    }
    do n = kevent(w->kq, NULL, 0, buf, lim, tsp); while (n < 0 && errno == EINTR);
    if (n <= 0) return n;
    for (i = 0; i < n; ++i) {
        int events = 0;
        if (buf[i].filter == EVFILT_READ) events |= TEAPOT_WAIT_IN;
        if (buf[i].filter == EVFILT_WRITE) events |= TEAPOT_WAIT_OUT;
        if (!events) continue;
        out[k].udata = buf[i].udata;
        out[k++].events = events;
    }
    return k;
}

void tp_wait_destroy(tp_wait *w)
{
    if (w->kq >= 0) close(w->kq);
    memset(w, 0, sizeof(*w));
    w->kq = -1;
}
#define TP_WAIT_READY 1
#endif
#endif
