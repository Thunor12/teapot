#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int failures;

static void ok(const char *name, int cond)
{
    if (cond) printf("[PASS] %s\n", name);
    else { printf("[FAIL] %s\n", name); failures++; }
}

static void test_bind_host_loopback(void)
{
    teapot_server server = {.port = 0, .bind_host = "127.0.0.1"};
    stb_teapot_socket_t ls = (stb_teapot_socket_t)-1;
    ok("bind_host open", teapot_listener_open(&server, &ls) == 0);
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    ok("getsockname", getsockname(ls, (struct sockaddr *)&addr, &len) == 0);
    ok("is loopback", addr.sin_addr.s_addr == htonl(INADDR_LOOPBACK));
    ok("port written back", server.port != 0);
    teapot_listener_close(ls);
}

static void test_bind_host_localhost_fails(void)
{
    teapot_server server = {.port = 0, .bind_host = "localhost"};
    stb_teapot_socket_t ls = (stb_teapot_socket_t)-1;
    ok("localhost fails", teapot_listener_open(&server, &ls) == -1);
}

struct stop_ctx { teapot_server *server; int rc; volatile int done; };

static void *listen_thread(void *arg)
{
    struct stop_ctx *ctx = arg;
    ctx->rc = teapot_listen(ctx->server);
    ctx->done = 1;
    return NULL;
}

static void test_listen_returns_when_stop_set(void)
{
    teapot_server server = {.port = 0, .bind_host = "127.0.0.1"};
    teapot_request_stop(&server);
    struct stop_ctx ctx = {.server = &server, .rc = -99, .done = 0};
    pthread_t th;
    ok("thread", pthread_create(&th, NULL, listen_thread, &ctx) == 0);
    int i;
    for (i = 0; i < 40 && !ctx.done; ++i) {
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 50000000L};
        nanosleep(&ts, NULL);
    }
    ok("listen returned", ctx.done == 1 && ctx.rc == 0);
    if (ctx.done) pthread_join(th, NULL);
}

int main(void)
{
    test_bind_host_loopback();
    test_bind_host_localhost_fails();
    test_listen_returns_when_stop_set();
    return failures ? 1 : 0;
}
