#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>

#ifdef _WIN32
int main(void)
{
    puts("unit_test_run_epoll is POSIX-only for now");
    return 0;
}
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int failures;

static void ok(const char *name, int cond)
{
    if (cond)
        printf("[PASS] %s\n", name);
    else
    {
        printf("[FAIL] %s\n", name);
        failures++;
    }
}

static teapot_response ping(const teapot_request *req)
{
    (void)req;
    return teapot_json(TEAPOT_HTTP_OK, "{\"ok\":true}");
}

struct run_ctx
{
    teapot_server *s;
    int rc;
};

static void *run_thread(void *arg)
{
    struct run_ctx *c = arg;
    c->rc = teapot_run(c->s);
    return NULL;
}

static void test_run_port0_ping_then_stop(void)
{
    teapot_route routes[] = {{TEAPOT_GET, "/ping", ping}};
    teapot_server srv = {
        .bind_host = "127.0.0.1",
        .port = 0,
        .routes = routes,
        .route_count = 1,
    };
    struct run_ctx ctx = {.s = &srv, .rc = -99};
    pthread_t th;
    ok("run thread", pthread_create(&th, NULL, run_thread, &ctx) == 0);
    int i, port = 0;
    for (i = 0; i < 100 && (port = srv.port) == 0; ++i)
    {
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000000L};
        nanosleep(&ts, NULL);
    }
    ok("ephemeral port", port != 0);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons((uint16_t)port)};
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    ok("connect", connect(fd, (struct sockaddr *)&a, sizeof(a)) == 0);
    const char *req = "GET /ping HTTP/1.1\r\n\r\n";
    ok("write", write(fd, req, strlen(req)) == (ssize_t)strlen(req));
    char buf[256];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0)
        n = 0;
    buf[n] = '\0';
    ok("200 json", strstr(buf, "HTTP/1.1 200") && strstr(buf, "{\"ok\":true}"));
    close(fd);

    teapot_request_stop(&srv);
    pthread_join(th, NULL);
    ok("run returned 0", ctx.rc == 0);
}

int main(void)
{
    test_run_port0_ping_then_stop();
    return failures ? 1 : 0;
}
#endif
