#define TEAPOT_RECV_TIMEOUT_MS 30000
#define TEAPOT_SEND_TIMEOUT_MS 200
#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
int main(void)
{
    puts("unit_test_send_timeout is POSIX-only for now");
    return 0;
}
#else
#include <sys/socket.h>
#include <unistd.h>

static teapot_response big_handler(const teapot_request *req)
{
    (void)req;
    /* Larger than typical socket buffers so send blocks if peer never reads. */
    enum { N = 512 * 1024 };
    char *body = (char *)malloc(N);
    if (!body)
    {
        teapot_response r;
        teapot_response_init(&r, TEAPOT_HTTP_INTERNAL_ERROR);
        return r;
    }
    memset(body, 'x', N);
    teapot_response r = teapot_bytes(TEAPOT_HTTP_OK, "application/octet-stream", body, N);
    free(body);
    return r;
}

int main(void)
{
    stb_teapot_socket_t fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0)
    {
        puts("[FAIL] socketpair");
        return 1;
    }

    int buf = 4096;
    (void)setsockopt(fds[0], SOL_SOCKET, SO_SNDBUF, &buf, sizeof(buf));
    (void)setsockopt(fds[1], SOL_SOCKET, SO_RCVBUF, &buf, sizeof(buf));

    const char *req = "GET /big HTTP/1.1\r\n\r\n";
    if (write(fds[1], req, strlen(req)) < 0)
    {
        puts("[FAIL] write request");
        teapot_close(fds[0]);
        teapot_close(fds[1]);
        return 1;
    }
    /* Peer never reads the response → write should hit TEAPOT_SEND_TIMEOUT_MS. */

    teapot_route routes[] = {
        {TEAPOT_GET, "/big", big_handler},
    };
    teapot_server server = {
        .port = 0,
        .routes = routes,
        .route_count = 1,
    };

    int rc = teapot_serve_client(&server, fds[0]);
    teapot_close(fds[0]);
    teapot_close(fds[1]);

    if (rc == -1)
    {
        puts("[PASS] blocked write times out");
        puts("\nALL TESTS PASSED");
        return 0;
    }
    printf("[FAIL] expected send timeout rc=-1, got rc=%d\n", rc);
    return 1;
}
#endif
