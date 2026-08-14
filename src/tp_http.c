#include "teapot.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

    static const char *teapot_status_to_str(int status)
    {
        switch (status)
        {
        case TEAPOT_HTTP_OK:
            return "OK";
        case TEAPOT_HTTP_CREATED:
            return "Created";
        case TEAPOT_HTTP_BAD_REQUEST:
            return "Bad Request";
        case TEAPOT_HTTP_NOT_FOUND:
            return "Not Found";
        case TEAPOT_HTTP_METHOD_NOT_ALLOWED:
            return "Method Not Allowed";
        case TEAPOT_HTTP_UNSUPPORTED_MEDIA_TYPE:
            return "Unsupported Media Type";
        case TEAPOT_HTTP_INTERNAL_ERROR:
            return "Internal Server Error";
        default:
            return "Unknown";
        }
    }

    int tp_sb_appendf(tp_string_builder *sb, const char *fmt, ...)
    {
        va_list args;

        va_start(args, fmt);
        int n = vsnprintf(NULL, 0, fmt, args);
        va_end(args);

        // NOTE: the new_capacity needs to be +1 because of the null terminator.
        // However, further below we increase sb->count by n, not n + 1.
        // This is because we don't want the sb to include the null terminator. The user can always sb_append_null() if they want it
        tp_da_reserve(sb, sb->count + (size_t)(n + 1));
        char *dest = sb->items + sb->count;
        va_start(args, fmt);
        vsnprintf(dest, (size_t)(n + 1), fmt, args);
        va_end(args);

        sb->count += (size_t)n;

        return n;
    }

    static int teapot_format_response(tp_string_builder *out, const teapot_response *resp)
    {
        if (!out || !resp)
            return -1;

        const char *ct = (resp->content_type != NULL) ? resp->content_type : "text/plain";
        if (strpbrk(ct, "\r\n") != NULL)
            return -1;

        tp_sb_appendf(out,
                      "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: " TP_SIZE_T_FMT "\r\nConnection: close\r\n\r\n",
                      resp->status, teapot_status_to_str(resp->status), ct, tp_da_len(resp->body));
        if (resp->body.count > 0)
            tp_sb_append_buf(out, resp->body.items, resp->body.count);
        return 0;
    }

    teapot_response teapot_text(int status, const char *s)
    {
        teapot_response r;
        teapot_response_init(&r, status);
        r.content_type = "text/plain";
        if (s)
            teapot_response_write(&r, s, strlen(s));
        return r;
    }

    teapot_response teapot_json(int status, const char *json)
    {
        teapot_response r;
        teapot_response_init(&r, status);
        r.content_type = "application/json";
        if (json)
            teapot_response_write(&r, json, strlen(json));
        return r;
    }

    teapot_response teapot_bytes(int status, const char *ctype, const void *p, size_t n)
    {
        teapot_response r;
        teapot_response_init(&r, status);
        r.content_type = ctype ? ctype : "text/plain";
        if (p && n > 0)
            teapot_response_write(&r, p, n);
        return r;
    }

    static teapot_handler teapot_find_handler(teapot_server *server, teapot_request *req)
    {
        const char *target = req->path.items ? req->path.items : "";
        size_t path_n = tp_request_target_path_n(target);
        for (size_t i = 0; i < server->route_count; i++)
        {
            const teapot_route *r = &server->routes[i];
            if (r->method != req->method)
                continue;
            size_t n = strlen(r->path);
            if (r->prefix)
            {
                if (n > 0 && r->path[n - 1] == '/' && path_n >= n && memcmp(r->path, target, n) == 0)
                    return r->handler;
            }
            else if (n == path_n && memcmp(r->path, target, n) == 0)
            {
                return r->handler;
            }
        }
        return NULL;
    }

    int teapot_recv_request(stb_teapot_socket_t client, char *buffer, int bufsize, int *out_received)
    {
        if (!teapot_socket_ok((stb_teapot_socket_t)client) || !buffer || bufsize <= 0)
        {
            return -1;
        }

        int received = teapot_read((stb_teapot_socket_t)client, buffer, bufsize - 1);
        if (received <= 0)
        {
            if (out_received)
            {
                *out_received = received;
            }
            return -1;
        }

        buffer[received] = '\0';
        if (out_received)
        {
            *out_received = received;
        }
        return 0;
    }

    int teapot_send_response(stb_teapot_socket_t client, const teapot_response *resp)
    {
        if (!teapot_socket_ok(client) || !resp)
            return -1;

        tp_string_builder out = {0};
        if (teapot_format_response(&out, resp) != 0)
        {
            tp_sb_free(out);
            return -1;
        }

        int rc = teapot_write_all(client, out.items, out.count);
        tp_sb_free(out);
        return rc;
    }

    int teapot_handle_client_connection(teapot_server *server, stb_teapot_socket_t client)
    {
        int rc = teapot_serve_client(server, client);
        teapot_close(client);
        return rc;
    }
