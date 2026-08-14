#define TEAPOT_USE_POLL
#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>

#ifdef _WIN32
int main(void)
{
    puts("unit_test_run is POSIX-only for now");
    return 0;
}
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
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

/* Connect then RST before accept consumes the handshake (ECONNABORTED race).
 * Bug: teapot_run disarmed listen forever on that error with an empty slab. */
static void test_run_survives_accept_rst_storm(void)
{
    teapot_route routes[] = {{TEAPOT_GET, "/ping", ping}};
    teapot_server srv = {
        .bind_host = "127.0.0.1",
        .port = 0,
        .routes = routes,
        .route_count = 1,
        .max_conns = 4,
    };
    struct run_ctx ctx = {.s = &srv, .rc = -99};
    pthread_t th;
    ok("rst storm thread", pthread_create(&th, NULL, run_thread, &ctx) == 0);
    int i, port = 0;
    for (i = 0; i < 100 && (port = srv.port) == 0; ++i)
    {
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000000L};
        nanosleep(&ts, NULL);
    }
    ok("rst storm port", port != 0);

    for (i = 0; i < 64; ++i)
    {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons((uint16_t)port)};
        struct linger lin = {.l_onoff = 1, .l_linger = 0};
        inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
        if (fd < 0)
            continue;
        (void)setsockopt(fd, SOL_SOCKET, SO_LINGER, &lin, sizeof(lin));
        (void)connect(fd, (struct sockaddr *)&a, sizeof(a));
        close(fd);
    }

    {
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 300000000L};
        nanosleep(&ts, NULL);
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons((uint16_t)port)};
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    ok("post-rst connect", connect(fd, (struct sockaddr *)&a, sizeof(a)) == 0);
    const char *req = "GET /ping HTTP/1.1\r\n\r\n";
    ok("post-rst write", write(fd, req, strlen(req)) == (ssize_t)strlen(req));

    char buf[256];
    ssize_t n = -1;
    for (i = 0; i < 50; ++i)
    {
        n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0)
            break;
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 50000000L};
        nanosleep(&ts, NULL);
    }
    if (n < 0)
        n = 0;
    buf[n] = '\0';
    ok("post-rst 200", n > 0 && strstr(buf, "HTTP/1.1 200") && strstr(buf, "{\"ok\":true}"));
    close(fd);

    teapot_request_stop(&srv);
    pthread_join(th, NULL);
    ok("rst storm run returned 0", ctx.rc == 0);
}

/* EMFILE on accept disarms listen; with an empty slab only a tick re-arm recovers. */
static void test_run_rearms_listen_after_emfile(void)
{
    struct rlimit rl, saved;
    ok("getrlimit", getrlimit(RLIMIT_NOFILE, &saved) == 0);
    rl = saved;
    rl.rlim_cur = 48;
    if (setrlimit(RLIMIT_NOFILE, &rl) != 0)
    {
        ok("setrlimit soft (skip)", 1);
        return;
    }

    teapot_route routes[] = {{TEAPOT_GET, "/ping", ping}};
    teapot_server srv = {
        .bind_host = "127.0.0.1",
        .port = 0,
        .routes = routes,
        .route_count = 1,
        .max_conns = 4,
    };
    struct run_ctx ctx = {.s = &srv, .rc = -99};
    pthread_t th;
    ok("emfile thread", pthread_create(&th, NULL, run_thread, &ctx) == 0);
    int i, port = 0;
    for (i = 0; i < 100 && (port = srv.port) == 0; ++i)
    {
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000000L};
        nanosleep(&ts, NULL);
    }
    ok("emfile port", port != 0);

    int dummy[64];
    int nd = 0;
    for (i = 0; i < 64; ++i)
    {
        int d = open("/dev/null", O_RDONLY);
        if (d < 0)
            break;
        dummy[nd++] = d;
    }
    ok("exhausted fds", nd > 1);
    /* Free one fd so the client can connect; accept still needs another → EMFILE. */
    close(dummy[--nd]);

    {
        int cfd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons((uint16_t)port)};
        inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
        ok("emfile probe connect", cfd >= 0 && connect(cfd, (struct sockaddr *)&a, sizeof(a)) == 0);
        {
            struct timespec ts = {.tv_sec = 0, .tv_nsec = 500000000L};
            nanosleep(&ts, NULL);
        }
        if (cfd >= 0)
            close(cfd);
    }

    for (i = 0; i < nd; ++i)
        close(dummy[i]);

    {
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 600000000L};
        nanosleep(&ts, NULL);
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons((uint16_t)port)};
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    ok("post-emfile connect", fd >= 0 && connect(fd, (struct sockaddr *)&a, sizeof(a)) == 0);
    const char *req = "GET /ping HTTP/1.1\r\n\r\n";
    ok("post-emfile write", write(fd, req, strlen(req)) == (ssize_t)strlen(req));

    char buf[256];
    ssize_t n = -1;
    {
        int fl = fcntl(fd, F_GETFL, 0);
        if (fl >= 0)
            (void)fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    }
    for (i = 0; i < 40; ++i)
    {
        n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0)
            break;
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 50000000L};
        nanosleep(&ts, NULL);
    }
    if (n < 0)
        n = 0;
    buf[n] = '\0';
    ok("post-emfile 200", n > 0 && strstr(buf, "HTTP/1.1 200") && strstr(buf, "{\"ok\":true}"));
    close(fd);

    teapot_request_stop(&srv);
    pthread_join(th, NULL);
    ok("emfile run returned 0", ctx.rc == 0);
    (void)setrlimit(RLIMIT_NOFILE, &saved);
}

static void test_sigpipe_on_early_peer_close(void)
{
    stb_teapot_socket_t fds[2];
    ok("sigpipe socketpair", socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);

    teapot_route routes[] = {{TEAPOT_GET, "/ping", ping}};
    teapot_server server = {.port = 0, .routes = routes, .route_count = 1};
    const char *req = "GET /ping HTTP/1.1\r\n\r\n";
    ok("sigpipe write req", write(fds[1], req, strlen(req)) == (ssize_t)strlen(req));
    close(fds[1]); /* peer gone before/during response write */

    int rc = teapot_serve_client(&server, fds[0]);
    teapot_close(fds[0]);
    ok("sigpipe serve fails cleanly", rc == -1);
}

int main(void)
{
    test_run_port0_ping_then_stop();
    test_run_survives_accept_rst_storm();
    test_run_rearms_listen_after_emfile();
    test_sigpipe_on_early_peer_close();
    return failures ? 1 : 0;
}
#endif
