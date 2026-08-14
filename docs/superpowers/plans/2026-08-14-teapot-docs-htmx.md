# Teapot docs + response headers Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship public response headers + `teapot_html`, an embed pipeline for offline docs, and five `teapot_docs_*` interactive htmx doc binaries with ink-forge dark-first UI.

**Architecture:** Library changes in `src/` amalgamated to `stb_teapot.h`. Docs assets live in `examples/docs/` and bake to `build/docs_embed.h` via `./nob embed`. Shared `examples/docs_app.h` + thin per-wait mains always call `teapot_run`.

**Tech Stack:** C17, stb-style amalgam, nob build, POSIX/Windows sockets, vendored htmx + IBM Plex woff2, htmx fragments.

**Spec:** `docs/superpowers/specs/2026-08-14-teapot-docs-htmx-design.md`  
**UI skill:** `.cursor/skills/teapot-docs-ui/SKILL.md`

## Global Constraints

- TDD: failing test before production code for library/embed behavior.
- Amalgam: edit `src/`, run `./nob amalgamate`; tests compile only `stb_teapot.h`.
- No CDN for any docs asset (htmx, CSS, JS, fonts).
- `/frag` handlers: no `sleep`; no request bytes in HTML/JSON/headers.
- Bind `127.0.0.1`; docs use `teapot_run` only.
- Do not commit `build/docs_embed.h`.
- Commits: focused, conventional (`feat:` / `fix:` / `test:` / `docs:`).
- Branch: `feat/docs-htmx` (not main).

---

### Task 1: Response header API + teapot_html

**Files:**
- Modify: `src/teapot.h` (`teapot_response`, init/free declarations)
- Modify: `src/tp_http.c` (header API, format, html helper)
- Modify: `src/tp_conn.c` (dispatch free-then-assign; free headers in response_free path)
- Modify: `tests/unit_test_response.c` (or create `tests/unit_test_response_headers.c` + `nob.c` entries)
- Regenerate: `stb_teapot.h`

**Interfaces:**
- Produces: `teapot_response_header`, `teapot_response_headerf`, `teapot_html`; `teapot_response.headers`; format emits extras after `Connection: close`

- [ ] **Step 1: Extend response unit tests (fail first)**

Cover (via `teapot_send_response` + socketpair, existing pattern):
1. empty name / `\r`/`\n` in name or value → header API `-1`
2. name with `:`, space, CTL → `-1`
3. oversize name/value → `-1`
4. empty value → `0`, present on wire
5. reserved any-case → `-1`; if forced into `headers`, format omits
6. two `Set-Cookie` both on wire, append order
7. `teapot_html` ctype + body; NULL html → empty body
8. empty headers → byte-identical to old `teapot_text` wire for `"OK"`

- [ ] **Step 2: Run tests — expect FAIL**

Run: `./nob amalgamate` then compile/run the response test binary (or `./nob` if wired).

- [ ] **Step 3: Implement**

- Add `tp_headers headers` to `teapot_response`.
- `teapot_response_init` zeroes headers; `teapot_response_free` calls `tp_headers_free` then frees body and zeroes.
- `tp_headers_append_owned` + validation per spec (token rule, CTLs in values, reserved reject).
- `teapot_response_header` / `headerf` / `teapot_html`.
- `teapot_format_response`: after Connection close, emit extras; skip reserved; keep content_type CRLF check.
- `teapot_text`/`json`/`bytes`/`html` use init (headers zeroed).
- Dispatch: remove init-before-assign; free then assign; 404 by value with empty headers.

- [ ] **Step 4: Amalgamate, run all unit tests, commit**

```bash
./nob amalgamate && ./nob
git add src/teapot.h src/tp_http.c src/tp_conn.c stb_teapot.h tests/ nob.c
git commit -m "feat: public response headers and teapot_html"
```

---

### Task 2: Embed generator + fixture tests

**Files:**
- Modify: `nob.c` (`embed` command, stale check, `-I build` for docs later)
- Create: `tests/fixtures/embed_docs/` (tiny html/js/css/woff2 stub)
- Create: `tests/unit_test_embed.c`
- Generate: `build/docs_embed.h`, `build/test_embed.h` (gitignored)

**Interfaces:**
- Produces: `tp_embed_find`, `tp_embed_file` in generated header (static)
- Consumes: none from Task 1 except build stays green

- [ ] **Step 1: Write embed unit test against fixture-generated header (fail first)**

Assert find hit (`len` + first bytes), miss → NULL. Wire `nob` to generate `build/test_embed.h` from fixtures before compiling the test.

- [ ] **Step 2: Implement `./nob embed`**

Walk rules from spec: `/` URLs, unsigned char blobs, durable statics, caps 512KiB/64/2MiB, whitelist html/htm/js/css/woff2, reject `..`/`\`/symlink/dot/unknown, sorted LF. Output `build/docs_embed.h` for `examples/docs/` (create minimal placeholder docs dir if empty so default `./nob` does not fail — or skip embed until Task 3 if `examples/docs` missing).

- [ ] **Step 3: Generator negative tests** (optional small C driver or nob self-check): oversize/unknown ext fail closed.

- [ ] **Step 4: `./nob` green; commit**

```bash
git add nob.c tests/fixtures/embed_docs tests/unit_test_embed.c
git commit -m "feat: nob embed for offline docs assets"
```

---

### Task 3: Docs static shell (ink forge)

**Files:**
- Create: page inventory under `examples/docs/` per spec
- Create: `examples/docs/css/theme.css`, `js/theme.js`, `vendor/htmx.min.js`, `fonts/*.woff2`
- Follow: `.cursor/skills/teapot-docs-ui/SKILL.md` tokens exactly

**Interfaces:**
- Consumes: embed generator from Task 2
- Produces: complete `examples/docs/` tree that embeds cleanly

- [ ] **Step 1: Vendor htmx (pin version in comment) + IBM Plex woff2 (SIL)**

Download into tree; no CDN references in HTML.

- [ ] **Step 2: theme.css + theme.js**

Dark default without `data-theme`; allowlist `dark`|`light`; setAttribute only; tokens from skill.

- [ ] **Step 3: HTML pages**

`index.html`, `api/*.html`, `wait/index.html` with chrome rules (landing toggle-only; inner nav). Placeholder copy OK if snippets are complete enough for success criteria 1/1b. Live htmx targets named regions for Task 4 frags.

- [ ] **Step 4: `./nob embed` succeeds; commit**

```bash
git add examples/docs
git commit -m "feat: ink-forge offline docs static assets"
```

---

### Task 4: docs_app + thin wait mains

**Files:**
- Create: `examples/docs_app.h`
- Create: `examples/teapot_docs_epoll.c` … `teapot_docs_wfmo.c`
- Modify: `nob.c` (build OS-gated docs binaries with `-I build`, depend on `docs_embed.h`)
- Modify: `README.md` (how to run; dark default; offline)

**Interfaces:**
- Consumes: Task 1 headers/html; Task 2 `tp_embed_find`; Task 3 pages
- Produces: runnable `./build/teapot_docs_poll` (and epoll on Linux)

- [ ] **Step 1: Implement `docs_app.h`**

Routes: exact `/frag/*` first (wait-meta, ping, responses toys, HX-Trigger literal); GET-only embed catch-all with aliases; strip `?` for find; `teapot_bytes` for embeds; port parse; SIGINT stop; always `teapot_run`.

- [ ] **Step 2: Thin mains** with `TEAPOT_USE_*` + `TEAPOT_DEMO_WAIT_NAME` + `STB_TEAPOT_IMPLEMENTATION`.

- [ ] **Step 3: Wire nob; manually smoke one binary** (`timeout 2` or curl after background — do not hang `./nob` test loop on docs binaries).

- [ ] **Step 4: README + commit**

```bash
git add examples/docs_app.h examples/teapot_docs_*.c nob.c README.md
git commit -m "feat: interactive teapot_docs binaries for each wait backend"
```

---

### Task 5: Final verification

- [ ] **Step 1:** `./nob` full unit suite green (incl. headers + embed).
- [ ] **Step 2:** Confirm amalgam diff clean after `./nob amalgamate`.
- [ ] **Step 3:** Quick curl smoke of `teapot_docs_poll` for `/`, `/api/`, `/wait/`, one `/frag/ping`.
- [ ] **Step 4:** No commit unless fixes needed; report ready for branch finish.

---

## Spec coverage checklist

| Spec area | Task |
|-----------|------|
| §1 headers / html / dispatch | 1 |
| §2 embed | 2 |
| §3 UX / frags / mains | 3–4 |
| §4 TDD contracts | 1–2 |
| §5 visual | 3 |
| README / CI build docs bins | 4–5 |
