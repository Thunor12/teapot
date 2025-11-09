#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"
#include <stdio.h>
#include <string.h>

// ----------------------------------------------------
// Route handlers
// ----------------------------------------------------
teapot_response hello_handler(const teapot_request *req)
{
    return (teapot_response){200, "Hello from GET /hello\n"};
}

teapot_response echo_handler(const teapot_request *req)
{
    static char buffer[1024 * 8];
    snprintf(buffer, sizeof(buffer),
             "POST /echo received!\nBody (%lld bytes):\n%s\n", req->body_length, req->body);
    return (teapot_response){200, buffer};
}

// ----------------------------------------------------
// Main
// ----------------------------------------------------
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

    printf("Starting stb_teapot POST test...\n");
    printf("  GET  -> http://localhost:8080/hello\n");
    printf("  POST -> http://localhost:8080/echo\n");
    printf("\nPress Ctrl+C to stop.\n\n");

    return teapot_listen(&server);
}
