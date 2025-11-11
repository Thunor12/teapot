#define STB_TEAPOT_IMPLEMENTATION
#include "../stb_teapot.h"
#include <stdio.h>
#include <string.h>

// ----------------------------------------------------
// Route handlers
// ----------------------------------------------------
teapot_response hello_handler(const teapot_request *req)
{
    (void)req; // Unused parameter

    teapot_response resp;
    teapot_response_init(&resp, 200);
    tp_sb_appendf(&resp.body, "Hello from GET /hello\n");

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
        return resp;
    }

    if (strcmp(req->content_type.items, "text/plain") != 0)
    {
        resp.status = 415;
        tp_sb_appendf(&resp.body,
                      "Unsupported Media Type (%s): Only text/plain is supported\n", req->content_type);

        return resp;
    }

    resp.status = 200;
    tp_sb_appendf(&resp.body,
                  "POST /echo received!\nBody (%zu bytes) %s:\n%s\n",
                  req->body_length, req->content_type, req->body.items);

    return resp;
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
