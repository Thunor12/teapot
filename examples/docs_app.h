#ifndef TEAPOT_DOCS_APP_H
#define TEAPOT_DOCS_APP_H

/*
 * Shared docs demo app. Thin mains define TEAPOT_USE_*, TEAPOT_DEMO_WAIT_NAME,
 * and STB_TEAPOT_IMPLEMENTATION before including this header.
 */
#include "../stb_teapot.h"
#include "docs_embed.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef TEAPOT_DEMO_WAIT_NAME
#error "TEAPOT_DEMO_WAIT_NAME must be defined before including docs_app.h"
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <wincon.h>
#ifndef CTRL_C_EVENT
#define CTRL_C_EVENT 0
#endif
#ifndef CTRL_BREAK_EVENT
#define CTRL_BREAK_EVENT 1
#endif
#ifndef CTRL_CLOSE_EVENT
#define CTRL_CLOSE_EVENT 2
#endif
#else
#include <signal.h>
#endif

static teapot_server *g_docs_srv;

static void docs_request_stop(void)
{
    if (g_docs_srv)
        teapot_request_stop(g_docs_srv);
}

#ifdef _WIN32
static BOOL WINAPI docs_console_handler(DWORD type)
{
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT)
    {
        docs_request_stop();
        return TRUE;
    }
    return FALSE;
}
#else
static void docs_on_sigint(int sig)
{
    (void)sig;
    docs_request_stop();
}
#endif

static int docs_parse_port(void)
{
    const char *e = getenv("TEAPOT_DEMO_PORT");
    char *end = NULL;
    unsigned long v;

    if (e == NULL || e[0] == '\0')
        return 8080;

    v = strtoul(e, &end, 10);
    if (end == e || *end != '\0' || v < 1ul || v > 65535ul)
    {
        fprintf(stderr, "TEAPOT_DEMO_PORT must be an integer 1..65535 (got %s)\n", e);
        exit(1);
    }
    return (int)v;
}

/* --- /frag handlers: compile-time literals only; no request bytes --- */

static teapot_response frag_wait_name(const teapot_request *req)
{
    (void)req;
    return teapot_html(TEAPOT_HTTP_OK, TEAPOT_DEMO_WAIT_NAME);
}

static teapot_response frag_wait_meta(const teapot_request *req)
{
    (void)req;
#ifdef TEAPOT_USE_WFMO
    return teapot_html(TEAPOT_HTTP_OK,
                       "<p><strong>This binary:</strong> <code>" TEAPOT_DEMO_WAIT_NAME
                       "</code> (<code>TEAPOT_USE_WFMO</code>).</p>"
                       "<p class=\"muted\">WFMO watches at most <strong>64</strong> sockets "
                       "(listen + clients). Prefer <code>TEAPOT_USE_WSAPOLL</code> for more "
                       "concurrency on Windows.</p>"
                       "<p class=\"muted\">Handlers run on the reactor thread — no "
                       "<code>sleep</code> in demos.</p>");
#else
    return teapot_html(TEAPOT_HTTP_OK,
                       "<p><strong>This binary:</strong> <code>" TEAPOT_DEMO_WAIT_NAME
                       "</code>.</p>"
                       "<p class=\"muted\">Handlers run on the reactor thread — no "
                       "<code>sleep</code> in demos; \"slow\" is client "
                       "<code>hx-trigger</code> delay.</p>");
#endif
}

static teapot_response frag_ping(const teapot_request *req)
{
    (void)req;
    return teapot_json(TEAPOT_HTTP_OK, "{\"ok\":true}");
}

static teapot_response frag_echo_toy(const teapot_request *req)
{
    (void)req;
    return teapot_html(TEAPOT_HTTP_OK, "<p>echo-toy ok (side-effect free)</p>");
}

static teapot_response frag_hx_trigger(const teapot_request *req)
{
    teapot_response r;
    (void)req;
    r = teapot_html(TEAPOT_HTTP_OK, "<p>HX-Trigger sent</p>");
    (void)teapot_response_header(&r, "HX-Trigger",
                                 "{\"teapot:flash\":{\"target\":\"#status-line\"}}");
    return r;
}

static teapot_response docs_embed_handler(const teapot_request *req)
{
    char path[512];
    size_t n = 0;
    size_t len;
    const char *raw;
    const tp_embed_file *f;

    raw = req->path.items ? req->path.items : "";
    while (raw[n] != '\0' && raw[n] != '?' && n + 1 < sizeof(path))
    {
        path[n] = raw[n];
        n++;
    }
    path[n] = '\0';

    if (strcmp(path, "/") == 0)
    {
        f = tp_embed_find("/index.html");
    }
    else if (strcmp(path, "/api") == 0)
    {
        f = tp_embed_find("/api/index.html");
    }
    else if (strcmp(path, "/wait") == 0)
    {
        f = tp_embed_find("/wait/index.html");
    }
    else
    {
        len = strlen(path);
        if (len > 0 && path[len - 1] == '/')
        {
            if (len + sizeof("index.html") <= sizeof(path))
            {
                memcpy(path + len, "index.html", sizeof("index.html"));
                f = tp_embed_find(path);
            }
            else
            {
                f = NULL;
            }
        }
        else
        {
            f = tp_embed_find(path);
        }
    }

    if (f == NULL)
        return teapot_text(TEAPOT_HTTP_NOT_FOUND, "Not Found\n");
    return teapot_bytes(TEAPOT_HTTP_OK, f->ctype, f->data, f->len);
}

static int teapot_docs_main(void)
{
    int port = docs_parse_port();
    int rc;
    teapot_route routes[] = {
        {TEAPOT_GET, "/frag/wait-name", frag_wait_name, 0},
        {TEAPOT_GET, "/frag/wait-meta", frag_wait_meta, 0},
        {TEAPOT_GET, "/frag/ping", frag_ping, 0},
        {TEAPOT_POST, "/frag/echo-toy", frag_echo_toy, 0},
        {TEAPOT_GET, "/frag/hx-trigger", frag_hx_trigger, 0},
        {TEAPOT_GET, "/", docs_embed_handler, 1},
    };
    teapot_server server = {
        .port = port,
        .routes = routes,
        .route_count = sizeof(routes) / sizeof(routes[0]),
        .bind_host = "127.0.0.1",
    };

    g_docs_srv = &server;
#ifdef _WIN32
    (void)SetConsoleCtrlHandler(docs_console_handler, TRUE);
#else
    (void)signal(SIGINT, docs_on_sigint);
#endif

    printf("teapot_docs (%s) http://127.0.0.1:%d/\n", TEAPOT_DEMO_WAIT_NAME, port);
    printf("Press Ctrl+C to stop.\n");

    rc = teapot_run(&server);
    if (rc != 0)
    {
        fprintf(stderr, "teapot_run failed (bind/listen). Try TEAPOT_DEMO_PORT=%d\n",
                port == 8080 ? 8081 : port + 1);
    }
    g_docs_srv = NULL;
    return rc;
}

#endif /* TEAPOT_DOCS_APP_H */
