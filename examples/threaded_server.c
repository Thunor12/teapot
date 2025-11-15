#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <signal.h>
#endif

typedef struct
{
    stb_teapot_socket_t client_socket;
    teapot_server server;
} threaded_server;

teapot_response hello_handler(const teapot_request *req)
{
    teapot_response resp;
    teapot_response_init(&resp, 200);

    /* Example: echo an X-Hello header if present (existence check) */
    tp_header_line h = {0};
    int r = tp_headers_check(&req->headers, "X-Hello", NULL, &h);
    if (r != TP_HEADER_NOT_FOUND)
    {
        tp_sb_appendf(&resp.body, "Hello (X-Hello=%s)\n", h.value.items ? h.value.items : "");
    }
    else
    {
        tp_sb_appendf(&resp.body, "Hello from GET /hello\n");
    }

    return resp;
}

teapot_response echo_handler(const teapot_request *req)
{
    teapot_response resp = {0};
    teapot_response_init(&resp, 200);

    if (req->body_length == 0)
    {
        resp.status = 400;
        tp_sb_appendf(&resp.body, "Bad Request: No body provided\n");
        tp_sb_append_null(&resp.body);
        return resp;
    }

    /* single-call check for existence + match */
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
                      "Unsupported Media Type (%s): Only text/plain is supported\n", hdr.value.items ? hdr.value.items : "");
        tp_sb_append_null(&resp.body);
        return resp;
    }

    resp.status = 200;
    tp_sb_appendf(&resp.body,
                  "POST /echo received!\nBody (%zu bytes) %s:\n%s\n",
                  req->body_length, hdr.value.items, req->body.items);
    tp_sb_append_null(&resp.body);

    return resp;
}

/* Worker entry: receives a teapot_socket_t* (heap-allocated), handles client and frees it. */
#ifdef _WIN32
static DWORD WINAPI client_thread_func(LPVOID arg)
{
    printf("Started thread %lu\n", GetCurrentThreadId());

    threaded_server *pc = (threaded_server *)arg;
    if (pc)
    {
        teapot_handle_client_connection(&pc->server, pc->client_socket); /* pass server pointer if needed */
        teapot_close(pc->client_socket);
        free(pc);
    }
    return 0;
}
#else
static void *client_thread_func(void *arg)
{
    threaded_server *pc = (threaded_server *)arg;
    if (pc)
    {
        teapot_handle_client_connection(&pc->server, pc->client_socket); /* pass server pointer if needed */
        teapot_close(pc->client_socket);
        free(pc);
    }
    return NULL;
}
#endif

int main(void)
{

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

    printf("threaded server listening on %d\n", server.port);

    while (1)
    {
        stb_teapot_socket_t client = teapot_listener_accept(listen_sock);
        if ((int)client < 0)
        {
            continue;
        }

        /* allocate socket on heap for thread arg (avoid race on stack) */
        threaded_server *parg = malloc(sizeof(threaded_server));
        if (!parg)
        {
            teapot_close((stb_teapot_socket_t)client);
            continue;
        }
        *parg = (threaded_server){.server = server, .client_socket = client};

#ifdef _WIN32
        HANDLE h = CreateThread(NULL, 0, client_thread_func, parg, 0, NULL);
        if (h)
        {
            CloseHandle(h); /* let thread run detached */
        }
        else
        {
            teapot_close((stb_teapot_socket_t)client);
            free(parg);
        }
#else
        pthread_t thr;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if (pthread_create(&thr, &attr, client_thread_func, parg) != 0)
        {
            teapot_close((stb_teapot_socket_t)client);
            free(parg);
        }
        pthread_attr_destroy(&attr);
#endif
    }

    /* unreachable in this simple example */
    teapot_close((stb_teapot_socket_t)listen_sock);
    return 0;
}