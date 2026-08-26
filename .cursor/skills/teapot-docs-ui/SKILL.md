---
name: teapot-docs-ui
description: >-
  Visual and interaction rules for teapot interactive docs (examples/docs).
  Use when editing docs HTML/CSS, theme toggle, landing/API/wait pages, or
  reviewing docs UI against the design spec.
---

# Teapot docs UI

## Intent

Interactive documentation for teapot: modern, bold, trustworthy. Teaches the
onramp (Guide), common API, and wait backends. Not a marketing site and not a
dashboard.

## Theme

- **Dark by default.** Light mode via an explicit toggle only.
- Persist `localStorage` key `teapot-docs-theme` (`dark`|`light` only; else dark).
- Dark until the user toggles. **Never** auto-follow `prefers-color-scheme`.
- Default CSS is dark with **no** `data-theme` required (avoid light FOUC).
  Inline `<head>` script: read storage, set `data-theme` via
  `document.documentElement.setAttribute` / `dataset` only — **never**
  interpolate storage into HTML/`innerHTML`.
- Toggle lives in chrome (not a floating badge on hero media).

## Tokens (lock)

| Token | Dark | Light |
|-------|------|-------|
| `--bg` | `#0B0C0E` | `#EEEFF2` |
| `--fg` | `#F2F3F5` | `#121317` |
| `--muted` | `#9AA0A8` | `#5C616A` |
| `--accent` | `#D4894A` | `#A85A20` (AA on `--bg`) |
| `--border` | `#2A2C30` | `#D5D7DC` |
| `--code-bg` | `#14161A` | `#E4E6EA` |

Body text uses `--fg`, not `--accent`.

## Visual direction — ink forge

Deep near-black surfaces, sharp type, single warm copper accent on **cool**
surfaces, high contrast. Serious C systems tool — not a SaaS template.

### Do

- Vendor **IBM Plex Sans** 400/600 (+ 700 for wordmark if needed) and **IBM
  Plex Mono** 400 as `.woff2` under `examples/docs/fonts/`. `@font-face` local
  URLs only. `font-display: swap`. **No font CDN.**
- CSS variables as above for both themes.
- Landing visual = large **teapot** wordmark (Plex Sans bold). Grain/vignette
  OK as texture only — not the main idea. No mesh-as-hero.
- Intentional motion (2–3): theme cross-fade, nav underline, fragment swap
  fade. No load-in hero animation.

### Don't

- Purple-on-white / purple-indigo gradients.
- Warm cream paper (`#F4F1EA`) + serif display + copper/terracotta.
- Broadsheet hairline newspaper layout.
- Cards in the hero; pill clusters; stat strips; emoji; glow stacks;
  `rounded-full` chip soup.
- Detached labels/badges overlaid on hero media.
- Inter/Roboto/Arial as the primary voice.
- Fraunces/Literata (or any warm-paper serif pairing) for this docs shell.

## Layout rules

- Brand first: “teapot” is hero-level on landing; inner pages carry brand in nav.
- Landing chrome: **theme toggle only** (hero owns **Guide (primary) · API · Wait**).
- Inner chrome: `teapot` + **Guide · API · Wait** + toggle.
- One job per section: one headline, one short supporting sentence.
- Cards only for live fragment toys. Wait comparison = table/snippet, not a card.
- Full-bleed atmosphere OK; no inset hero image cards.
- Narrow: stack CTAs; code blocks `overflow-x: auto` without page overflow.
- Guide is prose + one primary code sample — not a dashboard; keep toys on API/Wait.

## Accessibility

- Theme toggle keyboard-reachable; announce state.
- Contrast: body text WCAG AA against `--bg` in both themes.
- Focus rings visible on both themes (`--accent`).

## Copy

README-plain. No emoji. No “blazingly”. Show limits honestly.

## htmx

- Fragments inherit page CSS (no shadow DOM).
- Prefer `hx-swap="innerHTML"` into named targets; keep chrome outside swaps.
- No request-reflected HTML (security lock from design spec).
