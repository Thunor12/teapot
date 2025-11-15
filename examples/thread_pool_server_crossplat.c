#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
/* Winsock must be included before windows.h to avoid conflicts */
#include <winsock2.h>
#include <windows.h>
#include <wincon.h>

/* Fallbacks if the toolchain doesn't expose these (MinGW variants sometimes differ). */
#ifndef CTRL_C_EVENT
#define CTRL_C_EVENT 0
#endif
#ifndef CTRL_BREAK_EVENT
#define CTRL_BREAK_EVENT 1
#endif
#ifndef CTRL_CLOSE_EVENT
#define CTRL_CLOSE_EVENT 2
#endif

/* Ensure SetConsoleCtrlHandler prototype is available. If headers don't provide it, declare it. */
#ifndef _MSC_VER /* MSVC will already have it */
#ifndef SetConsoleCtrlHandler
BOOL WINAPI SetConsoleCtrlHandler(BOOL(WINAPI *HandlerRoutine)(DWORD), BOOL Add);
#endif
#endif

#else
#include <signal.h>
#include <pthread.h>
#include <poll.h>
#include <unistd.h>
#endif

/* ---------------- simple handlers ---------------- */
teapot_response hello_handler(const teapot_request *req)
{
    teapot_response resp;
    teapot_response_init(&resp, 200);

    tp_header_line h = {0};
    int r = tp_headers_check(&req->headers, "X-Hello", NULL, &h);
    if (r != TP_HEADER_NOT_FOUND)
        tp_sb_appendf(&resp.body, "Hello (X-Hello=%s)\n", h.value.items ? h.value.items : "");
    else
        tp_sb_appendf(&resp.body, "Hello from GET /hello\n");

    tp_sb_append_null(&resp.body);
    return resp;
}

teapot_response echo_handler(const teapot_request *req)
{
    teapot_response resp;
    teapot_response_init(&resp, 200);

    if (req->body_length == 0)
    {
        resp.status = 400;
        tp_sb_appendf(&resp.body, "Bad Request: No body provided\n");
        tp_sb_append_null(&resp.body);
        return resp;
    }

    tp_header_line hdr = {0};
    int res = tp_headers_check(&req->headers, "Content-Type", "text/plain", &hdr);
    if (res == TP_HEADER_NOT_FOUND)
    {
        resp.status = 400;
        tp_sb_appendf(&resp.body, "Bad Request: Missing Content-Type header\n");
        tp_sb_append_null(&resp.body);
        return resp;
    }
    if (res != TP_HEADER_MATCH)
    {
        resp.status = 415;
        tp_sb_appendf(&resp.body,
                      "Unsupported Media Type (%s): Only text/plain is supported\n",
                      hdr.value.items ? hdr.value.items : "");
        tp_sb_append_null(&resp.body);
        return resp;
    }

    resp.status = 200;
    tp_sb_appendf(&resp.body,
                  "POST /echo received!\nBody (%zu bytes) %s:\n%s\n",
                  req->body_length, hdr.value.items ? hdr.value.items : "", req->body.items ? req->body.items : "");
    tp_sb_append_null(&resp.body);
    return resp;
}

/* ---------------- job queue (cross-platform) ---------------- */
typedef struct job_s
{
    stb_teapot_socket_t client;
    struct job_s *next;
} job_t;

typedef struct
{
    job_t *head;
    job_t *tail;
    int shutdown;
#ifdef _WIN32
    CRITICAL_SECTION mutex;
    CONDITION_VARIABLE cond_var;
#else
    pthread_mutex_t m;
    pthread_cond_t cv;
#endif
} job_queue_t;

static job_queue_t g_queue;

static void job_queue_init(job_queue_t *q)
{
    q->head = q->tail = NULL;
    q->shutdown = 0;
#ifdef _WIN32
    InitializeCriticalSection(&q->mutex);
    InitializeConditionVariable(&q->cond_var);
#else
    pthread_mutex_init(&q->m, NULL);
    pthread_cond_init(&q->cv, NULL);
#endif
}

static void job_queue_push(job_queue_t *q, stb_teapot_socket_t client)
{
    job_t *j = (job_t *)malloc(sizeof(job_t));
    if (!j)
    {
        return;
    }

    j->client = client;
    j->next = NULL;
#ifdef _WIN32
    EnterCriticalSection(&q->mutex);
    if (q->tail)
    {
        q->tail->next = j;
    }
    else
    {
        q->head = j;
    }
    q->tail = j;
    LeaveCriticalSection(&q->mutex);
    WakeConditionVariable(&q->cond_var);
#else
    pthread_mutex_lock(&q->m);
    if (q->tail)
    {
        q->tail->next = j;
    }
    else
    {
        q->head = j;
    }
    q->tail = j;
    pthread_cond_signal(&q->cv);
    pthread_mutex_unlock(&q->m);
#endif
}

static int job_queue_pop(job_queue_t *q, stb_teapot_socket_t *out_client)
{
#ifdef _WIN32
    EnterCriticalSection(&q->mutex);
    while (!q->head && !q->shutdown)
        SleepConditionVariableCS(&q->cond_var, &q->mutex, INFINITE);
    if (q->shutdown && !q->head)
    {
        LeaveCriticalSection(&q->mutex);
        return 0;
    }
    job_t *j = q->head;
    q->head = j->next;
    if (!q->head)
    {
        q->tail = NULL;
    }
    LeaveCriticalSection(&q->mutex);
#else
    pthread_mutex_lock(&q->m);
    while (!q->head && !q->shutdown)
        pthread_cond_wait(&q->cv, &q->m);
    if (q->shutdown && !q->head)
    {
        pthread_mutex_unlock(&q->m);
        return 0;
    }
    job_t *j = q->head;
    q->head = j->next;
    if (!q->head)
        q->tail = NULL;
    pthread_mutex_unlock(&q->m);
#endif
    *out_client = j->client;
    free(j);
    return 1;
}

static void job_queue_shutdown(job_queue_t *q)
{
#ifdef _WIN32
    EnterCriticalSection(&q->mutex);
    q->shutdown = 1;
    LeaveCriticalSection(&q->mutex);
    WakeAllConditionVariable(&q->cond_var);
#else
    pthread_mutex_lock(&q->m);
    q->shutdown = 1;
    pthread_cond_broadcast(&q->cv);
    pthread_mutex_unlock(&q->m);
#endif
}

/* ---------------- worker ---------------- */
typedef struct
{
    teapot_server *server;
} worker_arg_t;

#ifdef _WIN32
static DWORD WINAPI worker_thread_fn(LPVOID arg)
{
    printf("Worker thread started: %lu\n", GetCurrentThreadId());

    worker_arg_t *w = (worker_arg_t *)arg;
    stb_teapot_socket_t client;
    while (job_queue_pop(&g_queue, &client))
    {
        if ((int)client < 0)
        {
            continue;
        }
        teapot_handle_client_connection(w->server, client);
    }
    return 0;
}
#else
static void *worker_thread_fn(void *arg)
{
    printf("Worker thread started: %lu\n", (unsigned long)pthread_self());

    worker_arg_t *w = (worker_arg_t *)arg;
    stb_teapot_socket_t client;
    while (job_queue_pop(&g_queue, &client))
    {
        if ((int)client < 0)
            continue;
        teapot_handle_client_connection(w->server, client);
    }
    return NULL;
}
#endif

/* ---------------- graceful shutdown ---------------- */
static volatile int g_terminate = 0;

#ifdef _WIN32
static BOOL WINAPI console_handler(DWORD ctrlType)
{
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT || ctrlType == CTRL_CLOSE_EVENT)
    {
        g_terminate = 1;
        job_queue_shutdown(&g_queue);
        return TRUE;
    }
    return FALSE;
}
#else
static void sigint_handler(int sig)
{
    (void)sig;
    g_terminate = 1;
    job_queue_shutdown(&g_queue);
}
#endif

/* ---------------- main ---------------- */
int main(void)
{
    /* routes */
    teapot_route routes[] = {
        {TEAPOT_GET, "/hello", hello_handler},
        {TEAPOT_POST, "/echo", echo_handler},
    };

    teapot_server server = {
        .port = 8080,
        .routes = routes,
        .route_count = sizeof(routes) / sizeof(routes[0]),
    };

    stb_teapot_socket_t listen_sock;
    if (teapot_listener_open(&server, &listen_sock) < 0)
    {
        fprintf(stderr, "failed to open listener\n");
        return 1;
    }

    printf("thread-pool cross-platform server listening on %d\n", server.port);

    job_queue_init(&g_queue);

    const int WORKER_COUNT = 4;
#ifdef _WIN32
    /* allocate worker handles */
    HANDLE *workers = (HANDLE *)malloc((size_t)WORKER_COUNT * sizeof(HANDLE));
    worker_arg_t warg = {.server = &server};
    for (int i = 0; i < WORKER_COUNT; ++i)
    {
        workers[i] = CreateThread(NULL, 0, worker_thread_fn, &warg, 0, NULL);
        if (!workers[i])
        {
            fprintf(stderr, "CreateThread failed\n");
        }
    }
    SetConsoleCtrlHandler(console_handler, TRUE);
#else
    pthread_t *workers = (pthread_t *)malloc((size_t)WORKER_COUNT * sizeof(pthread_t));
    worker_arg_t warg = {.server = &server};
    for (int i = 0; i < WORKER_COUNT; ++i)
    {
        pthread_create(&workers[i], NULL, worker_thread_fn, &warg);
    }
    signal(SIGINT, sigint_handler);
#endif

    /* accept loop using poll/WSAPoll */
    while (!g_terminate)
    {
#ifdef _WIN32
        WSAPOLLFD pfd;
        pfd.fd = (SOCKET)listen_sock;
        pfd.events = POLLIN;
        int rv = WSAPoll(&pfd, 1, 500); /* timeout ms */
        if (rv < 0)
        {
            break;
        }

        if (rv == 0)
        {
            continue;
        }

        if (pfd.revents & POLLIN)
        {
            stb_teapot_socket_t client = teapot_listener_accept(listen_sock);
            if ((int)client >= 0)
                job_queue_push(&g_queue, client);
        }
#else
        struct pollfd pfd = {.fd = (int)listen_sock, .events = POLLIN};
        int rv = poll(&pfd, 1, 500);
        if (rv < 0)
            break;
        if (rv == 0)
            continue;
        if (pfd.revents & POLLIN)
        {
            stb_teapot_socket_t client = teapot_listener_accept(listen_sock);
            if ((int)client >= 0)
                job_queue_push(&g_queue, client);
        }
#endif
    }

    /* shutdown and join workers */
    job_queue_shutdown(&g_queue);
#ifdef _WIN32
    for (int i = 0; i < WORKER_COUNT; ++i)
    {
        if (workers[i])
        {
            WaitForSingleObject(workers[i], INFINITE);
            CloseHandle(workers[i]);
        }
    }
    free(workers);
#else
    for (int i = 0; i < WORKER_COUNT; ++i)
        pthread_join(workers[i], NULL);
    free(workers);
    teapot_close((stb_teapot_socket_t)listen_sock);
#endif

    teapot_close((stb_teapot_socket_t)listen_sock);
    printf("server stopped\n");
    return 0;
}