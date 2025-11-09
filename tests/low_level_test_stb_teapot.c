#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"
#include <stdio.h>

// --------------------------
// Route handler
// --------------------------
teapot_response hello_handler(const teapot_request *req)
{
    return (teapot_response){200, "Hello Teapot\n"};
}

// --------------------------
// Main
// --------------------------
int main(void)
{
    // User-provided route array
    teapot_route routes[] = {
        {TEAPOT_GET, "/hello", hello_handler},
    };

    // Server configuration
    teapot_server server = {
        .port = 8080,
        .routes = routes,
        .route_count = sizeof(routes) / sizeof(routes[0])};

    printf("Starting server...\n");
    return teapot_listen(&server);
}
