# Teapot ideas / backlog

Scratch and parked product ideas. Specs and plans under `docs/superpowers/` win when they exist.

## Done

| Idea | Landed |
|------|--------|
| Interactive htmx docs per wait backend + common API | Spec/plan `*-teapot-docs-htmx-*`, `./nob doc`, Guide · API · Wait |

## Parked (no active plan)

| Idea | Notes |
|------|--------|
| Keep-alive | Still `Connection: close` only; reopen only with a framing/spec pass |
| Disk-serve DEV mode | Docs stay embedded (`./nob embed`); no live FS serve in demos |
| CDN htmx | Rejected for interactive docs (offline / vendored pin) |

## Specced, not this backlog

| Idea | Where |
|------|--------|
| Templates engine (Askama-style build-time HTML → C builders) | `specs/2026-08-14-teapot-templates-design.md` — after HTTP runtime audit |
