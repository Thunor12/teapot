# `./nob doc` + Guide · API · Wait docs

**Date:** 2026-08-14  
**Status:** Approved for implementation (brainstorm locks + expert revise/ship-with-nits incorporated)  
**Branch intent:** two PRs (A then B)

## Goal

1. **`./nob doc`** — ensure platform-default interactive docs binary, then process-replace into it.
2. **Entry-friendly docs IA** — Guide (onramp) · API (reference) · Wait (backends).

## Delivery split

| PR | Scope |
|----|--------|
| **A** | `./nob doc` + README Interactive docs update only |
| **B** | Guide page, chrome/landing CTAs, thin API overview, `/guide` 302, skill updates (`teapot-docs-ui`, `teapot-docs-content`) |

---

## §1 — `./nob doc` (PR A)

### Behavior

```
./nob doc
```

1. `NOB_GO_REBUILD_URSELF_PLUS` (existing; preserves argv).
2. Amalgamate `stb_teapot.h`.
3. `embed_if_stale` for docs embed only (`examples/docs` → `build/docs_embed.h`). Skip test embed / unit tests / other examples.
4. Compile **only** the platform-default docs binary (reuse compile helper; **must** `nob_procs_flush` and fail closed before launch).
5. Print to stderr/stdout: backend name, binary path, `http://127.0.0.1:<port>/` (port from `TEAPOT_DEMO_PORT` parse rules mirroring `docs_app.h`, default **8080** — print the resolved port; do not invent CLI args).
6. Process-replace into that binary. On success, nob does not return.

Any step failure → nonzero exit, **do not** launch.

Default `./nob` (no args) still must **not** start the docs server.

### Platform default (single `#ifdef` map drives compile + exec)

| Platform | Binary path |
|----------|-------------|
| Linux | `./build/teapot_docs_epoll` |
| macOS / FreeBSD / OpenBSD / NetBSD / DragonFly | `./build/teapot_docs_kqueue` |
| Other POSIX | `./build/teapot_docs_poll` |
| Windows | `./build/teapot_docs_wsapoll.exe` |

WFMO and non-default bins remain buildable via full `./nob` only.

### Exec rules (acceptance)

- **Fixed path only** — same string compile wrote. Prefer `execv("./build/…")`.
- **Never** `system(...)`, **never** `execvp` / PATH lookup, **never** binary name from argv/env in v1.
- **POSIX:** `execv` after flush.
- **Windows:** `_execv` / `_P_OVERLAY`-style replace so Ctrl+C goes to docs_app; if overlay unavailable, spawn + `exit(child_code)` documented as the fallback — parent must not remain a long-lived waiter that owns the console differently without documenting it.
- **CWD:** repo root (same as `./nob`). Failure messages may say so. No chdir magic in v1.

### README

- Primary path: `./nob doc`.
- One line: CI / correctness = `./nob`; interactive docs = `./nob doc`.
- Keep mention that OS-gated `teapot_docs_*` also exist under `build/` for non-default backends.
- Port: `TEAPOT_DEMO_PORT` (1..65535) or 8080.

---

## §2 — Docs IA (PR B)

### Nav / landing

- Inner chrome: **Guide · API · Wait** + brand + theme toggle.
- Landing chrome: theme-only; hero CTAs: **Guide (primary)**, API, Wait (secondary).
- Soft-pedal wait in hero lead (≤ one clause).
- Update `teapot-docs-ui` to match (today still says API|Wait only).

### Content ownership

| Layer | Owns | Must not |
|-------|------|----------|
| **Guide** | First copy-paste `teapot_listen` main; bind `127.0.0.1` not `"localhost"`; curl check; one-line listen→Wait handoff; links to API + Wait | Wait matrix, limits deep-dive, all response helpers, htmx toys, TLS |
| **API** | Reference: routes, responses, limits, listen-vs-run **table**; stub or “see Guide” instead of full first-program main | Second canonical full ping main |
| **Wait** | Backends, macros, live toys | Replacing Guide as onramp |

Redirect: bare `/guide` → `/guide/` (same pattern as `/api`, `/wait`).

### Skills

- `teapot-docs-content` — prose/IA (already drafted; keep ownership rules).
- `teapot-docs-ui` — visual; patch CTA/nav locks in PR B.
- Personal `documentation-and-adrs` — optional for ADRs; not required for HTML copy.

---

## §3 — Non-goals (v1)

- CLI port / wait-backend flags for `nob doc`.
- Background daemon mode.
- Merging Guide into landing HTML.
- Changing library public API.

## Success criteria

| # | Check |
|---|--------|
| 1 | From repo root, `./nob doc` serves docs on loopback after ensure-build |
| 2 | Ctrl+C stops the docs server cleanly (POSIX exec path) |
| 3 | Build failure does not exec |
| 4 | Guide page teaches one listen main; API overview does not duplicate it |
| 5 | `/guide` 302 → `/guide/`; chrome shows three pillars |

## Expert gates (pre-impl)

- Docs IA audit: **revise** → ownership + Windows + split PRs (incorporated).
- Thermo: **ship-with-nits** → fixed-path exec, flush, `/guide` 302, Windows replace (incorporated).
