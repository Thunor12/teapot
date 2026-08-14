#define TEAPOT_RECV_TIMEOUT_MS 200
#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>

#ifdef _WIN32
int main(void)
{
    puts("unit_test_timeout is POSIX-only for now");
    return 0;
}
#else
#include <sys/socket.h>
#include <unistd.h>

static int handler_called;

static teapot_response timeout_handler(const teapot_request *req)
{
    (void)req;
    handler_called = 1;
    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);
    teapot_response_write(&resp, "ok", 2);
    return resp;
}

int main(void)
{
    stb_teapot_socket_t fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0)
    {
        puts("[FAIL] socketpair");
        return 1;
    }

    const char *partial = "POST /echo HTTP/1.1\r\nContent-Length: 10\r\n\r\nabc";
    if (write(fds[1], partial, strlen(partial)) < 0)
    {
        puts("[FAIL] write");
        teapot_close(fds[0]);
        teapot_close(fds[1]);
        return 1;
    }
    /* No SHUT_WR: without a recv timeout, serve would block until the peer closed. */

    teapot_route routes[] = {
        {TEAPOT_POST, "/echo", timeout_handler},
    };
    teapot_server server = {
        .port = 0,
        .routes = routes,
        .route_count = 1,
    };

    handler_called = 0;
    int rc = teapot_serve_client(&server, fds[0]);
    teapot_close(fds[0]);
    teapot_close(fds[1]);

    if (rc == -1 && handler_called == 0)
    {
        puts("[PASS] incomplete body times out as 400");
        puts("\nALL TESTS PASSED");
        return 0;
    }
    printf("[FAIL] expected timeout rc=-1 handler=0, got rc=%d handler=%d\n", rc, handler_called);
    return 1;
}
#endif
