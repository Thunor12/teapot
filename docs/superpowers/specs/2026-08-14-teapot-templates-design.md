# Teapot Optional HTML Templates (Askama analogue)

**Date:** 2026-08-14  
**Status:** locked; implement **after** the HTTP runtime plan  
**Depends on:** `2026-08-14-teapot-listener-builder-design.md`  
**Goal:** Optional compile-time HTML helpers. A server that never renders HTML never touches this. Not part of the axum/actix rival surface.

## Position

Teapot’s product is an HTTP application server. Askama is optional next to axum/actix. Teapot templates are optional here.

- **Not** in `stb_teapot.h`. No `tp_html_escape` in the amalgam.
- **Not** required to build tests, fuzzing, or `examples/basic_server.c`.
- Default `./nob` does **not** run template codegen, even if `templates/` exists.
- `./nob templates` reads flat `templates/*.html` (no subdirectories) and writes **`build/tp_templates.h`** (gitignored).

## Constraints (locked)

- Include `stb_teapot.h` **before** `tp_templates.h` (needs `tp_string_builder` / `tp_sb_append_buf`).
- `tp_html_escape` is `static` in the generated file only. Escapes `& < > "` and `'` (`&amp; &lt; &gt; &quot; &#39;`). NULL `s` appends nothing.
- `{{ ident }}` HTML-escaped. `{{ ident|safe }}` and `{{ ident | safe }}` (whitespace around `|`) unescaped; document as dangerous.
- Optional whitespace inside `{{ }}`. `ident` is `[A-Za-z_][A-Za-z0-9_]*`. Unknown syntax or `{{` without `}}` → generator fails the build.
- File basename without `.html` must be a C identifier. `hello.html` → `tp_tmpl_hello`. `404.html` / `user-profile.html` → generator fail.
- One function per file: `void tp_tmpl_<name>(tp_string_builder *out, ...)` with a `const char *` per unique ident, **first-appearance order**. Each use emits escape or passthrough of that argument. NULL arg → empty.
- v1 syntax: literal text + `{{ ident }}` + `|safe` only. No `{% if %}`, `{% for %}`, `{% include %}`.
- Generator emits literals as C string chunks with every special byte escaped, or as `unsigned char` arrays + `tp_sb_append_buf`. Quotes, backslashes, and newlines in the template must not break the generated C.
- No request-time template interpreter.

## Usage

```c
#include "stb_teapot.h"
#include "tp_templates.h"

static teapot_response page(const teapot_request *req)
{
    (void)req;
    teapot_response r = teapot_bytes(TEAPOT_HTTP_OK, "text/html; charset=utf-8", NULL, 0);
    tp_tmpl_hello(&r.body, "world");
    return r;
}
```

`examples/html_server.c` is optional and only compiled when `build/tp_templates.h` exists.

## Tests (separate binary, later plan)

- `tests/unit_test_templates.c` is **not** in `unit_test_request.c`.
- Generator fixture `tests/templates/hello.html` with `<` and quotes → generated header → `tp_tmpl_hello` output contains `&lt;` not `<`.
- `|safe` passthrough is a second test.
- Default `./nob` still ignores missing app `templates/`.

## Out of scope

- Jinja control flow, inheritance, streaming, JSON templating
- Shipping generated HTML inside the amalgam
