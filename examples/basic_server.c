#define STB_TEAPOT_IMPLEMENTATION
#include "demo_handlers.h"

#include <signal.h>
#include <stdio.h>

static teapot_server *g_srv;

static void on_sigint(int sig)
{
    (void)sig;
    if (g_srv)
        teapot_request_stop(g_srv);
}

int main(void)
{
    teapot_route routes[] = {
        {TEAPOT_GET, "/ping", ping_handler},
        {TEAPOT_GET, "/hello", hello_handler},
        {TEAPOT_POST, "/echo", echo_handler},
    };

    teapot_server server = {
        .port = 8080,
        .routes = routes,
        .route_count = sizeof(routes) / sizeof(routes[0]),
        .bind_host = "127.0.0.1",
    };
    g_srv = &server;
    (void)signal(SIGINT, on_sigint);

    printf("Starting stb_teapot...\n");
    printf("  GET  -> http://127.0.0.1:8080/ping\n");
    printf("  GET  -> http://127.0.0.1:8080/hello\n");
    printf("  POST -> http://127.0.0.1:8080/echo\n");
    printf("\nPress Ctrl+C to stop.\n\n");

    return teapot_listen(&server);
}
