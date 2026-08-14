#include "teapot.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <sys/time.h>
#endif

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

    static int teapot_complete_request_body(stb_teapot_socket_t client, teapot_request *req, size_t content_length)
    {
        if (req->body_length >= content_length)
            return 0;
        req->body.count = req->body_length;
        while (req->body_length < content_length)
        {
            char read_buf[4096];
            size_t to_read = content_length - req->body_length;
            if (to_read > sizeof(read_buf))
                to_read = sizeof(read_buf);
            int n = teapot_read(client, read_buf, (int)to_read);
            if (n <= 0)
                return -1;
            tp_sb_append_buf(&req->body, read_buf, (size_t)n);
            req->body_length += (size_t)n;
        }
        tp_sb_append_null(&req->body);
        return 0;
    }

    static void teapot_send_status_body(stb_teapot_socket_t client, int status, const char *msg)
    {
        teapot_response resp;
        teapot_response_init(&resp, status);
        tp_sb_appendf(&resp.body, "%s", msg);
        (void)teapot_send_response(client, &resp);
        teapot_response_free(&resp);
    }

    static teapot_handler teapot_find_handler(teapot_server *server, teapot_request *req)
    {
        for (size_t i = 0; i < server->route_count; i++)
        {
            const teapot_route *r = &server->routes[i];
            if (r->method != req->method)
                continue;
            if (r->prefix)
            {
                size_t n = strlen(r->path);
                if (n > 0 && r->path[n - 1] == '/' && strncmp(r->path, req->path.items, n) == 0)
                    return r->handler;
            }
            else if (strcmp(r->path, req->path.items) == 0)
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

        const char *ct = (resp->content_type != NULL) ? resp->content_type : "text/plain";
        if (strpbrk(ct, "\r\n") != NULL)
            return -1;

        tp_string_builder header = {0};
        tp_sb_appendf(&header,
                      "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: " TP_SIZE_T_FMT "\r\nConnection: close\r\n\r\n",
                      resp->status, teapot_status_to_str(resp->status), ct, tp_da_len(resp->body));

        int rc = teapot_write_all(client, header.items, header.count);
        if (rc == 0 && resp->body.count > 0)
            rc = teapot_write_all(client, resp->body.items, resp->body.count);

        tp_sb_free(header);
        return rc;
    }

    int teapot_serve_client(teapot_server *server, stb_teapot_socket_t client)
    {
        if (!server || !teapot_socket_ok(client))
            return -1;

#if TEAPOT_RECV_TIMEOUT_MS > 0
#ifdef _WIN32
        DWORD ms = (DWORD)TEAPOT_RECV_TIMEOUT_MS;
        (void)setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)&ms, sizeof(ms));
#else
        struct timeval tv = {.tv_sec = (time_t)(TEAPOT_RECV_TIMEOUT_MS / 1000),
                             .tv_usec = (suseconds_t)((TEAPOT_RECV_TIMEOUT_MS % 1000) * 1000)};
        (void)setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
#endif

        char buffer[8192] = {0};
        int received = 0;
        if (teapot_recv_request(client, buffer, (int)sizeof(buffer), &received) < 0)
            return -1;

        teapot_request req = {0};
        size_t content_length = 0;
        if (parse_request(buffer, (size_t)received, &req, &content_length) < 0)
        {
            free_request(&req);
            teapot_send_status_body(client, TEAPOT_HTTP_BAD_REQUEST, "400 Bad Request\n");
            return -1;
        }

        if (teapot_complete_request_body(client, &req, content_length) != 0)
        {
            free_request(&req);
            teapot_send_status_body(client, TEAPOT_HTTP_BAD_REQUEST, "400 Bad Request\n");
            return -1;
        }

        teapot_handler handler = teapot_find_handler(server, &req);
        teapot_response resp;
        teapot_response_init(&resp, TEAPOT_HTTP_OK);
        if (handler)
            resp = handler(&req);
        else
        {
            resp.status = TEAPOT_HTTP_NOT_FOUND;
            tp_sb_appendf(&resp.body, "404 Not Found\n");
        }

        int rc = teapot_send_response(client, &resp);
        teapot_response_free(&resp);
        free_request(&req);
        return rc;
    }

    int teapot_handle_client_connection(teapot_server *server, stb_teapot_socket_t client)
    {
        int rc = teapot_serve_client(server, client);
        teapot_close(client);
        return rc;
    }
