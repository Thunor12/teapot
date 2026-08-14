#ifndef TEAPOT_DEMO_HANDLERS_H
#define TEAPOT_DEMO_HANDLERS_H

#include "../stb_teapot.h"

static teapot_response ping_handler(const teapot_request *req)
{
    (void)req;
    return teapot_json(TEAPOT_HTTP_OK, "{\"ok\":true}");
}

static teapot_response hello_handler(const teapot_request *req)
{
    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);

    tp_header_line h = {0};
    tp_header_result r = tp_headers_check(&req->headers, "X-Hello", NULL, &h);
    if (r != TP_HEADER_NOT_FOUND)
        tp_sb_appendf(&resp.body, "Hello (X-Hello=%s)\n", h.value.items ? h.value.items : "");
    else
        tp_sb_appendf(&resp.body, "Hello from GET /hello\n");
    return resp;
}

static teapot_response echo_handler(const teapot_request *req)
{
    teapot_response resp;
    teapot_response_init(&resp, TEAPOT_HTTP_OK);

    if (req->body_length == 0)
    {
        resp.status = TEAPOT_HTTP_BAD_REQUEST;
        tp_sb_appendf(&resp.body, "Bad Request: No body provided\n");
        return resp;
    }

    tp_header_line hdr = {0};
    tp_header_result res = tp_headers_check(&req->headers, "Content-Type", "text/plain", &hdr);
    if (res == TP_HEADER_NOT_FOUND)
    {
        resp.status = TEAPOT_HTTP_BAD_REQUEST;
        tp_sb_appendf(&resp.body, "Bad Request: Missing Content-Type header\n");
        return resp;
    }
    if (res != TP_HEADER_MATCH)
    {
        resp.status = TEAPOT_HTTP_UNSUPPORTED_MEDIA_TYPE;
        tp_sb_appendf(&resp.body, "Unsupported Media Type (%s): Only text/plain is supported\n",
                      hdr.value.items ? hdr.value.items : "");
        return resp;
    }

    tp_sb_appendf(&resp.body, "POST /echo received!\nBody (%zu bytes) %s:\n%s\n",
                  req->body_length, hdr.value.items ? hdr.value.items : "",
                  req->body.items ? req->body.items : "");
    return resp;
}

#endif
