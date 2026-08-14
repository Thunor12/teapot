---
name: teapot-docs-content
description: >-
  Writing rules for teapot interactive docs copy and structure under
  examples/docs. Use when authoring or editing Guide/API/Wait HTML prose,
  code samples, nav IA, or reviewing docs content for newcomers vs reference.
---

# Teapot docs content

Companion to `teapot-docs-ui` (look/feel). This skill owns **what to say** and
**in what order**. Prefer UI skill for tokens, chrome, and motion.

## Audience & progressive disclosure

Three layers — do not mix jobs on one page:

| Layer | Nav | Job |
|-------|-----|-----|
| **Guide** | Guide | **Owns the first copy-paste `teapot_listen` main** — get a working server in minutes |
| **API** | API | Reference only (routes, responses, limits, listen vs run table). **No full first-program main** — stub + “see Guide” if needed |
| **Wait** | Wait | `teapot_run` backends, compile-time macros, this-binary live toys |

**Authority:** Guide = first program; API = reference; Wait = backends + toys. Do not leave three canonical ping samples (README may stay short; Guide is the interactive onramp).

Landing stays a short brand + three CTAs (**Guide primary**, API/Wait secondary). No tutorial dump on the hero. Soft-pedal wait on the landing lead (one clause max). Guide’s closing beat: one sentence listen → Wait handoff (“one client → listen; overlapping I/O → Wait”).

## Voice

- Direct, second person (“define a route”, not “one might define”).
- Short sentences. Prefer tables over long paragraphs for comparisons.
- Name real symbols exactly as in `stb_teapot.h` / `src/teapot.h`.
- **Never invent APIs.** If unsure, read the header / examples first.
- Match README facts: bind host is dotted IPv4 (`127.0.0.1`), not `localhost`;
  TLS/HTTP2 out of scope; HTML is a content type, not the product.

## Guide (entry)

Minimum path on one page (or a tiny guide index + one page):

1. Drop `stb_teapot.h` (or amalgamate via `./nob`).
2. Minimal `teapot_listen` + one `GET` handler returning `teapot_json` / text.
3. Build & run note; point at `./nob doc` for these interactive docs.
4. One “next” link each to API (responses/routes) and Wait (`teapot_run`).

Code blocks: complete, copy-pasteable C; `#include` paths that match the repo
layout readers will use. Prefer the same snippet shape as README quick start
unless the guide intentionally teaches something else.

## API (detail)

- Keep subnav: Overview · Routes · Responses · Limits (extend only if a real
  public surface lands).
- Overview = listen vs run + orientation; not a second tutorial.
- Responses document `teapot_json` / `teapot_text` / `teapot_bytes` /
  `teapot_html` and response headers (`teapot_response_header` /
  `teapot_response_headerf`) only as they exist in the library.
- Live htmx toys stay on Responses/Wait; Guide stays static prose + code.

## Wait (backends)

- Explain compile-time `TEAPOT_USE_*` and that each docs binary is one backend.
- Live meta/toys OK; comparison table for epoll/poll/kqueue/wsapoll/wfmo.
- Do not claim a backend the current host did not build.

## `./nob doc` mentions

When docs mention how to open this site locally, prefer:

```sh
./nob doc
```

Platform-default wait binary (Linux epoll, BSD/macOS kqueue, other POSIX poll,
Windows WSAPoll). Port: `TEAPOT_DEMO_PORT` or 8080. Do not list every binary
as the primary path.

## Anti-patterns

- “Task N” / WIP scaffolding in user-visible copy.
- Marketing fluff, emoji, or dashboard chrome in prose.
- Duplicating the full API inside Guide.
- Relative same-dir links from bare `/api` or `/wait` URLs (servers must
  redirect to trailing slash; authors should still prefer `api/index.html`
  style links from the landing).

## Review checklist

- [ ] Newcomer can finish Guide without opening API.
- [ ] Every code sample compiles against current amalgam (or notes deps).
- [ ] Nav: Guide · API · Wait (+ brand + theme) on inner pages.
- [ ] No invented symbols; reserved headers / limits match library.
- [ ] `./nob doc` documented where “how do I run docs?” appears (README + Guide).
