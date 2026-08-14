# `./nob doc` + Guide docs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Ship `./nob doc` (PR A) then Guide·API·Wait IA (PR B) per `docs/superpowers/specs/2026-08-14-nob-doc-guide-design.md`.

**Architecture:** One `#ifdef` map → compile path + `execv`/`_execv` fixed path. Docs IA: Guide owns first program; API thinned; `/guide` 302.

**Tech stack:** C17, nob.h, existing `teapot_docs_*` + `docs_app.h`, static HTML under `examples/docs/`.

---

## File map

| File | Role |
|------|------|
| `nob.c` | `doc` subcommand, platform map, flush, exec |
| `README.md` | `./nob doc` primary path |
| `examples/docs/guide/index.html` | Onramp |
| `examples/docs/index.html` | Landing CTAs |
| `examples/docs/api/*.html`, `wait/index.html` | Nav + API thin |
| `examples/docs_app.h` | `/guide` redirect |
| `.cursor/skills/teapot-docs-ui/SKILL.md` | CTA/nav locks |
| `.cursor/skills/teapot-docs-content/SKILL.md` | already mostly done |

---

### Task 1: PR A — `./nob doc`

**Files:** `nob.c`, `README.md`

- [ ] Add `docs_default_src()` / `docs_default_out()` from one platform map
- [ ] `run_doc`: amalgamate → embed docs → compile one → flush → print URL → `execv`/`_execv`
- [ ] Wire `argv[0]=="doc"` early like `embed`
- [ ] README Interactive docs section
- [ ] Manual: `./nob doc` then curl `/` → 200; Ctrl+C
- [ ] Commit: `feat: add ./nob doc to launch platform-default docs`

### Task 2: PR B — Guide IA

**Files:** docs HTML, `docs_app.h`, skills

- [ ] Add `examples/docs/guide/index.html` (listen main)
- [ ] Thin API overview (remove full main → point Guide)
- [ ] Nav/landing: Guide · API · Wait; Guide primary CTA
- [ ] `/guide` → `/guide/` 302
- [ ] Update `teapot-docs-ui`
- [ ] `./nob` + `./nob doc` smoke `/guide/`
- [ ] Commit: `docs: add Guide onramp and three-pillar nav`

### Task 3: Final audit

- [ ] Expert re-review of diff vs spec
- [ ] Fix High/Medium if any
