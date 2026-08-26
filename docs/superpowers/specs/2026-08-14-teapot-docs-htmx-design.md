# Teapot interactive docs + response headers

**Date:** 2026-08-14  
**Status:** Ready for implementation planning (expert nits incorporated)  
**Source idea:** [`docs/superpowers/ideas.md`](../ideas.md) — htmx demos per wait backend + common API docs

## Goal

Ship:

1. A **public response-header API** (and `teapot_html`) so apps and demos can emit `HX-*`, `Location`, CORS, `Set-Cookie`, etc.
2. **Interactive documentation** as five wait-specific binaries sharing one app: common API docs + “this wait backend” docs, offline, with vendored htmx.
3. Docs UI that is **modern, bold, trustworthy** — **dark by default**, with a light/dark toggle.

Non-goals (parked in [`docs/superpowers/ideas.md`](../ideas.md)): templates engine, keep-alive, CDN htmx, disk-serve DEV mode.

## Success criteria

From **one** `./build/teapot_docs_*` binary:

| # | User can | Page |
|---|----------|------|
| 1 | Copy a complete `TEAPOT_USE_*` + `teapot_run` main | `/wait/` |
| 1b | Copy a complete `teapot_listen` ping main | `/api/` |
| 2 | See `hx-get`/`hx-post` swap **and** one `HX-Trigger` (status line / accent flash, not a toast) | `/api/responses.html` |
| 3 | Read WFMO-64 / prefer WSAPoll **in every binary** (comparison table; extra callout if this binary is WFMO) | `/wait/` |
| 4 | Toggle dark↔light with no layout/contrast break | chrome, all pages |
| 5 | No second wait binary, **no CDN (including fonts)**, no disk DEV | embed + vendored woff2 |

---

## §1 — Public response header API

### Shape

```c
typedef struct {
    int status;
    const char *content_type; /* borrowed; NULL => "text/plain"; must outlive send */
    tp_headers headers;       /* owned extra headers */
    tp_string_builder body;
} teapot_response;
```

### Public API

```c
int teapot_response_header(teapot_response *r, const char *name, const char *value);
int teapot_response_headerf(teapot_response *r, const char *name, const char *fmt, ...);
teapot_response teapot_html(int status, const char *html); /* NULL html => empty body */
```

| Symbol | Behavior |
|--------|----------|
| `teapot_response_header` | Append one header; **copies** name/value; `0` / `-1` (incl. OOM) |
| `teapot_response_headerf` | Format value, then same validation; `fmt` is trusted (printf rule) |
| `teapot_html` | `text/html; charset=utf-8` + body (sibling of `text`/`json`/`bytes`) |
| `teapot_response_init` | Zero status/ctype/`headers`/body (no allocations) |
| `teapot_response_free` | `tp_headers_free` + free body; zero both; never free `content_type` |

No htmx-specific helpers. No `teapot_redirect` in this work. **Defer** 3xx/204 status enums until a demo needs redirects (use `HX-*` toys only).

### Validation

- Reject empty name.
- Reject any `\r` or `\n` in name or value.
- Reject CTLs in **values** (bytes `< 0x20` except optional HTAB `0x09`, plus `0x7F`), not only CR/LF.
- Name token rule: reject if any byte is `<= 0x20`, `0x7F`, or `':'`.
- Apply `TP_MAX_HEADER_NAME_LEN` / `TP_MAX_HEADER_VALUE_LEN`.
- Empty value allowed.
- OOM on append → `-1`.
- Borrowed `content_type` still rejected on `\r`/`\n` at format time (existing rule).
- `teapot_text` / `json` / `bytes` / `html` zero `headers` via `teapot_response_init`.

### Reserved names (case-insensitive)

`Content-Type`, `Content-Length`, `Connection`, `Transfer-Encoding`.

- `teapot_response_header*` returns `-1`.
- `teapot_format_response` **skips** them if present (defense in depth; struct is public).

`Content-Type` stays the dedicated borrowed field.

### Semantics

- **Append-only**; duplicates allowed (`Set-Cookie`, repeated `HX-Trigger`).
- Wire order: status → `Content-Type` → `Content-Length` → `Connection: close` → extras in append order → blank line → body.
- Empty `headers` ⇒ **byte-identical** to today for `json`/`text`/`bytes`.

### Ownership / dispatch

- Handlers return by value (move). Library frees via `teapot_conn_free` → `teapot_response_free`.
- Dispatch:

```c
teapot_response_free(&c->res);
c->res = handler ? handler(&c->req) : teapot_not_found(); /* or inline 404 by value */
```

Never `init` then assign. `teapot_response_free` must leave a zeroed empty response.

- Internal helper `tp_headers_append_owned(tp_headers *, name, value)` for the response API (copy + NUL + size caps). Request parser stays parse-then-append; do not force one function to do both jobs.
- Document: one live response; free before replacing on error paths.

---

## §2 — Embed pipeline

### Source of truth

`examples/docs/` — HTML, CSS, JS, **vendored fonts** (`.woff2`), vendored `htmx.min.js` (version pin in a comment + README).

### Generator

`./nob embed` (also from default `./nob` when docs are newer) writes **`build/docs_embed.h`** (not committed). Demos compile with `-I build` (or an equivalent locked include path in `docs_app.h`).

```c
typedef struct {
    const char *path;   /* URL path, e.g. "/api/index.html" */
    const char *ctype;
    const char *data;   /* points at unsigned char blob */
    size_t len;
} tp_embed_file;

/* static table + static find in the generated header */
static const tp_embed_file *tp_embed_find(const char *path);
```

### Rules

- Emit `static const unsigned char blob_N[] = { ... };` + `len = sizeof`.
- Table fields `path` / `ctype` / `data` are **durable statics** in `docs_embed.h` (no generator temporaries).
- URL paths with `/` only; map `examples/docs/api/index.html` → `/api/index.html`.
- Reject `\`, `.`, `..`, empty segments; no symlinks; skip dotfiles and dot-directories.
- Caps (hard): per-file **512 KiB**; max **64** files; total payload **2 MiB**; fail closed.
- Extension whitelist only:
  - `.html`/`.htm` → `text/html; charset=utf-8`
  - `.js` → `application/javascript`
  - `.css` → `text/css`
  - `.woff2` → `font/woff2`
  - unknown ext → generator error
- Deterministic: sorted paths, LF only, no timestamps.
- `tp_embed_*` never enters `stb_teapot.h`.
- `tp_embed_find`: exact match on path after `?` strip; no trailing-slash normalize except demo-core aliases.

### Serving

- Lookup key = router path slice (strip `?` like routing).
- Serve with `teapot_bytes(status, ctype, data, len)` only — never `strlen`/`teapot_html` on blobs.
- Embed catch-all, `/`, and dir aliases are **`TEAPOT_GET` only**. POST only on explicit `/frag/*` toys.
- Demo-core aliases:
  - `/` → `/index.html`
  - path ending in `/` → that path + `index.html` (e.g. `/api/` → `/api/index.html`)
  - bare `/api` and `/wait` (no trailing slash) → `/api/index.html` and `/wait/index.html`
- Exact `/frag/...` C routes **before** prefix `/` catch-all; never embed a `frag/` directory; catch-all 404s `/frag/*` misses.

### Deps

`embed_if_stale(examples/docs → build/docs_embed.h)`; demos depend on `stb_teapot.h` + that header.

### Generator tests

Use fixture tree `tests/fixtures/embed_docs/` → `build/test_embed.h` (do **not** couple unit tests to full `examples/docs/`).

---

## §3 — Demo app UX

### Binaries

| Binary | Macro |
|--------|--------|
| `teapot_docs_epoll` | `TEAPOT_USE_EPOLL` |
| `teapot_docs_poll` | `TEAPOT_USE_POLL` |
| `teapot_docs_kqueue` | `TEAPOT_USE_KQUEUE` |
| `teapot_docs_wsapoll` | `TEAPOT_USE_WSAPOLL` |
| `teapot_docs_wfmo` | `TEAPOT_USE_WFMO` |

Each thin `examples/teapot_docs_*.c`: define wait macro + `TEAPOT_DEMO_WAIT_NAME` + `STB_TEAPOT_IMPLEMENTATION` + `#include "docs_app.h"`. **Always `teapot_run`.** One TU so the wait backend is real.

### Layout

```
examples/
  docs/                 # static source (theme CSS, pages, htmx, fonts)
  docs_app.h            # routes, /frag handlers, embed serve, teapot_docs_main()
  teapot_docs_epoll.c … # thin mains
  demo_handlers.h       # unchanged; basic/epoll/threaded only — not docs
```

### Page inventory (`examples/docs/`)

```
index.html              /                 landing
api/index.html          /api/             listen vs run + listen snippet
api/routes.html         /api/routes.html
api/responses.html      /api/responses.html  helpers + headers + live htmx toys
api/limits.html         /api/limits.html  timeouts, HTTP subset, Connection: close, socket ownership
wait/index.html         /wait/            run snippet, comparison table, WFMO, live ping
css/theme.css
js/theme.js
vendor/htmx.min.js      # version pin in file comment
fonts/*.woff2           # IBM Plex Sans 400/600 (+700 optional) + Plex Mono 400
```

Landing chrome: **theme toggle only** (hero owns API | Wait CTAs). One landing sentence names **this** wait backend (from `TEAPOT_DEMO_WAIT_NAME`) — not a stat strip.

Inner chrome: `teapot` + API + Wait + theme toggle.

Wait page: one HTML shell + `/frag/wait-meta` (compile-time strings). Comparison = **table/snippet**, not a card. Live ping fragment. **No** `sleep` in handlers — “slow” = client `hx-trigger` delay; page states handlers run on the reactor thread.

### `/frag` routes (C handlers, not embedded)

Closed literals only: `wait-meta`, `ping`, responses GET/POST toys, one `HX-Trigger` endpoint.

### Security locks (demos)

- `/frag` HTML/JSON/headers: compile-time literals / closed wait-name set only.
- **No request bytes** in HTML, JSON, or extra headers.
- No `Access-Control-Allow-Origin`. No `Location`/`HX-Redirect` from request data.
- POST toys side-effect free.

### Runtime

- Bind `127.0.0.1`; port unset → 8080; else `strtoul` full-string **1..65535** or stderr + exit 1.
- Print wait name + URL on start; bind fail hints `TEAPOT_DEMO_PORT`.
- SIGINT → `teapot_request_stop` (null-check; set `g_srv` before `signal`). Windows: also console ctrl handler.

### CI

Build OS-allowed `teapot_docs_*`; **do not** run them in the unit-test loop.

---

## §4 — Testing & README

### Headers (unit — TDD contract)

Prefer asserting formatted bytes (expose/test via existing `teapot_send_response` + socketpair pattern, or a testable format path — pick one in the plan; do not invent a second public formatter).

1. Empty name / `\r`/`\n` in name or value → `-1`
2. Name with `:`, space, or CTL → `-1`
3. Oversize name/value → `-1`
4. Empty value → `0`, header present on wire
5. Reserved (any case) → `-1`; if forced into `headers`, format **omits** them
6. Two `Set-Cookie` / two `HX-Trigger` both appear, append order preserved
7. `teapot_html` → ctype + body; `NULL` html → empty body
8. Empty `headers` → **byte-identical** to today’s `text`/`json`/`bytes` wire
9. `headerf` covered if kept in API

### Embed (unit — TDD contract)

1. Generator on `tests/fixtures/embed_docs/`: path map, sorted LF, ctypes locked
2. Reject `..`, `\`, empty segment, symlink, dotfile, unknown ext, oversize → non-zero exit
3. Unit against generated test header: hit (`len` + first N bytes), miss → `NULL`
4. Serving aliases + `/frag` order: docs_app / manual when authoring — not required in `./nob` unit loop

### CI / README

| Kind | Coverage |
|------|----------|
| CI | `./nob` amalgamate + embed-if-stale + unit tests (valgrind when present) |
| Manual | Authoring pages: run one docs binary + click/curl |

README: edit `examples/docs/` → `./nob` → `./build/teapot_docs_*`; pin htmx version; list OS-gated binaries; note dark default + theme toggle; offline (no CDN).

---

## §5 — Visual design (docs UI)

Follow project skill `.cursor/skills/teapot-docs-ui/SKILL.md` (tokens, fonts, do/don't).

### Theme

- **Dark by default**; light via chrome toggle only.
- Default CSS is dark without requiring `data-theme` (no light FOUC). Inline `<head>` script reads `teapot-docs-theme` and sets `data-theme` when needed.
- Persist **only** `dark`|`light`; any other value → treat as dark.
- Apply theme only via `document.documentElement.setAttribute("data-theme", …)` / `dataset` — **never** interpolate `localStorage` into HTML/`innerHTML`.
- **Never** `prefers-color-scheme`.
- Light `--bg` is cool gray (`#EEEFF2` class), **not** `#F4F1EA`. Copper accent only on cool surfaces.

### Direction — “ink forge”

- Landing visual = large **teapot** wordmark (IBM Plex Sans bold). Grain/vignette as texture only — not the idea. No mesh-as-hero.
- Fonts: **IBM Plex Sans + IBM Plex Mono** vendored `.woff2` only (one foundry).
- Motion (2–3): theme cross-fade, nav affordance, fragment fade. No load-in hero animation.
- Avoid: purple gradients, cream+serif+terracotta, broadsheet, hero cards, pill/stat soup, emoji, glow stacks.

### Implementation notes

- Theme CSS + toggle JS + fonts under `examples/docs/`, embedded. **No CDN.**
- htmx swaps content regions only; chrome (nav + theme toggle) outside swap roots.

---

## File / ownership map

| Area | Owner |
|------|--------|
| Response headers + `teapot_html` + format | `src/teapot.h`, `src/tp_http.c`, `src/tp_conn.c` (dispatch), tests, amalgam |
| Embed generator | `nob.c` → `build/docs_embed.h` |
| Embed fixtures / tests | `tests/fixtures/embed_docs/`, `build/test_embed.h`, unit test |
| Header unit tests | extend `tests/unit_test_response.c` or `tests/unit_test_response_headers.c` |
| Docs app | `examples/docs_app.h`, `examples/teapot_docs_*.c`, `examples/docs/**` |
| UI skill | `.cursor/skills/teapot-docs-ui/SKILL.md` |

## Expert review log (design phase)

| Section | Thermo | Product | Quality |
|---------|--------|---------|---------|
| §1 headers | approve-with-nits (incorporated) | approve-with-nits | approve-with-nits |
| §2 embed | approve-with-nits | approve-with-nits | approve-with-nits (`build/` not committed) |
| §3 UX | approve-with-nits | approve-with-nits | approve-with-nits |
| §4 tests | approve-with-nits | revise→page/TDD map | revise → expanded TDD contract |
| §5 visual | approve-with-nits (theme allowlist) | revise → IBM Plex + cool light + inventory | revise → offline fonts, no CDN |

## Out of scope

Templates engine; keep-alive; CDN (htmx or fonts); disk-serve DEV mode; path params; query helpers; owning `content_type`; second header type; htmx-specific C helpers; 3xx/204 enums until a demo needs them.
