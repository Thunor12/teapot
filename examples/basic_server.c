#define STB_TEAPOT_IMPLEMENTATION
#include "demo_handlers.h"

#include <stdio.h>

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

    printf("Starting stb_teapot...\n");
    printf("  GET  -> http://localhost:8080/hello\n");
    printf("  POST -> http://localhost:8080/echo\n");
    printf("\nPress Ctrl+C to stop.\n\n");

    return teapot_listen(&server);
}
