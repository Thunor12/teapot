#include "teapot.h"

#if TEAPOT_WAIT == TEAPOT_WAIT_WFMO
#ifndef _WIN32
#error "TEAPOT_WAIT_WFMO requires Windows"
#else
#define TEAPOT_WAIT_IN 1
#define TEAPOT_WAIT_OUT 2
#define TP_WFMO_MAX 64
/* FD_CLOSE always when registered; FD_WRITE is edge-triggered (see select). */
#define TP_WFMO_MASK(e) \
    (((e) & TEAPOT_WAIT_IN ? (FD_ACCEPT | FD_READ) : 0L) | \
     ((e) & TEAPOT_WAIT_OUT ? FD_WRITE : 0L) | \
     ((e) ? FD_CLOSE : 0L))

typedef struct { void *udata; int events; } tp_wait_event;
typedef struct {
    WSAEVENT ev[TP_WFMO_MAX];
    stb_teapot_socket_t fd[TP_WFMO_MAX];
    void *udata[TP_WFMO_MAX];
    int interest[TP_WFMO_MAX];
    int count;
} tp_wait;

static int tp_wait_find(tp_wait *w, stb_teapot_socket_t fd)
{
    int i;
    for (i = 0; i < w->count; ++i)
        if (w->fd[i] == fd) return i;
    return -1;
}

/* FD_WRITE is edge-triggered: arm the event so an already-writable socket wakes. */
static int tp_wfmo_select(stb_teapot_socket_t fd, WSAEVENT ev, int events)
{
    if (WSAEventSelect(fd, ev, TP_WFMO_MASK(events)) != 0) return -1;
    if (events & TEAPOT_WAIT_OUT) (void)WSASetEvent(ev);
    return 0;
}

int tp_wait_create(tp_wait *w) { memset(w, 0, sizeof(*w)); return 0; }

int tp_wait_add(tp_wait *w, stb_teapot_socket_t fd, int events, void *udata)
{
    WSAEVENT ev;
    if (w->count >= TP_WFMO_MAX) return -1;
    ev = WSACreateEvent();
    if (ev == WSA_INVALID_EVENT) return -1;
    if (tp_wfmo_select(fd, ev, events) != 0) {
        WSACloseEvent(ev);
        return -1;
    }
    w->ev[w->count] = ev;
    w->fd[w->count] = fd;
    w->udata[w->count] = udata;
    w->interest[w->count++] = events;
    return 0;
}

int tp_wait_mod(tp_wait *w, stb_teapot_socket_t fd, int events, void *udata)
{
    int i = tp_wait_find(w, fd);
    if (i < 0) return -1;
    if (tp_wfmo_select(fd, w->ev[i], events) != 0) return -1;
    w->udata[i] = udata;
    w->interest[i] = events;
    return 0;
}

int tp_wait_del(tp_wait *w, stb_teapot_socket_t fd)
{
    int i = tp_wait_find(w, fd);
    if (i < 0) return -1;
    (void)WSAEventSelect(fd, NULL, 0);
    WSACloseEvent(w->ev[i]);
    w->count--;
    if (i != w->count) {
        w->ev[i] = w->ev[w->count];
        w->fd[i] = w->fd[w->count];
        w->udata[i] = w->udata[w->count];
        w->interest[i] = w->interest[w->count];
    }
    return 0;
}

int tp_wait_wait(tp_wait *w, int timeout_ms, tp_wait_event *out, int max_out)
{
    DWORD to = timeout_ms < 0 ? WSA_INFINITE : (DWORD)timeout_ms, n;
    WSANETWORKEVENTS ne;
    int idx, events = 0;
    if (w->count == 0) {
        if (timeout_ms < 0) Sleep(INFINITE);
        else Sleep(to);
        return 0;
    }
    n = WSAWaitForMultipleEvents((DWORD)w->count, w->ev, FALSE, to, FALSE);
    if (n == WSA_WAIT_TIMEOUT) return 0;
    if (n == WSA_WAIT_FAILED) return -1;
    idx = (int)(n - WSA_WAIT_EVENT_0);
    if (idx < 0 || idx >= w->count) return -1;
    /* Enum resets the event; on failure ResetEvent so we do not busy-loop. */
    if (WSAEnumNetworkEvents(w->fd[idx], w->ev[idx], &ne) != 0) {
        (void)ResetEvent(w->ev[idx]);
        return 0;
    }
    if (ne.lNetworkEvents & (FD_ACCEPT | FD_READ | FD_CLOSE)) events |= TEAPOT_WAIT_IN;
    if (ne.lNetworkEvents & FD_WRITE) events |= TEAPOT_WAIT_OUT;
    /* Synthetic WSASetEvent for already-writable OUT interest. */
    if (!events && (w->interest[idx] & TEAPOT_WAIT_OUT)) events = TEAPOT_WAIT_OUT;
    if (!events || max_out < 1) return 0;
    out[0].udata = w->udata[idx];
    out[0].events = events;
    return 1;
}

void tp_wait_destroy(tp_wait *w)
{
    int i;
    for (i = 0; i < w->count; ++i) {
        (void)WSAEventSelect(w->fd[i], NULL, 0);
        WSACloseEvent(w->ev[i]);
    }
    memset(w, 0, sizeof(*w));
}
#define TP_WAIT_READY 1
#endif
#endif
