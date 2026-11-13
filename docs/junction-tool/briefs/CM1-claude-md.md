# CM1: src/CLAUDE.md: the junction tool's current rules as present-tense facts (engineer, 2026-11-13)

Write one file: /home/pete/OpenTTD/src/CLAUDE.md (new, untracked; the orchestrator reviews and commits it). Change nothing else. Read-only otherwise. Keep your context small: read each source once, with offset/limit for long files.

## Why (for you, not for the file)
The engineer: "a condensed version of all the decisions we've made to date, stripped of all temporal/why information and presented as present tense facts. This should aid the agents working on the tool in a way that the large size of the decision log now makes it hard to do." It lives in src/ ("the CLAUDE.md should live with the code"): the junction code is src/junction_*.{h,cpp} and src/tests/junction_*.cpp, and Claude Code loads src/CLAUDE.md whenever an agent reads a file under src/.

## Sources (read in this order)
1. /home/pete/OpenTTD/docs/junction-tool/DECISIONS.md (all ~240 rows, D1..D224: later rows refine or supersede earlier ones; a row that says "superseded", "revises", "refined", "DROPPED" or is contradicted by a later row is reported in its CURRENT form only).
2. /home/pete/OpenTTD/docs/junction-tool/DESIGN-G1.md (the current pipeline: §7-§17; earlier DESIGN-*.md files describe superseded pipelines: use them only where G1 points to them).
3. /home/pete/OpenTTD/docs/junction-tool/TESTS.md (tags, budgets, jt modes) and tools/junction/jt.
4. /home/pete/OpenTTD/docs/junction-tool/STATUS.md line 2-3 only (what is current), CARDS-crossings.md (open questions).
5. Skim the latest briefs' "State" sections (docs/junction-tool/briefs/ newest 25 by mtime) for facts merged since DESIGN-G1 was last revised (tunnels TU1-TU4, lane pairing D221, topology window D222, crossings D223/D224, optional S-bends are NOT merged: TS1 parked).

## Format
- Title line, then sections in this order: Scope and files; Pipeline (stage names and what each owns, one line each); Notation and topology; Graph, order and structure (rows, joiners, merges, crossings, over/under); Bridges and tunnels; Placement (relations as families, facts, choices, edits); Heights (levels, foundations, terraform, cone, ramp rule, bridge heads, tunnels); Signals and prio; Hover and GUI (window, presets, tooltip, near search, cancel, build, spent/quoted); Game rules the model mirrors (cite the game function: GetRailFoundation, CheckBridgeSlope, CmdBuildTunnel...); Testing (identity tests, tags, budgets, jt modes, what every step runs, red-first); Working rules for agents (worktrees, no git stash, never touch the engineer's game, rig ports 4077/4079, one GUI client, normal config, absolute paths, commit message format, report format pointer); Open (undecided questions, each one line with its card).
- Each fact: one present-tense sentence (or a short bullet), no dates, no quotes, no "because"/"why", ending with its decision id(s) in brackets, e.g. "A crossing may be a tunnel or a bridge; doubled main-line crossings are bridges (D220, D174)." Facts from DESIGN-G1 or briefs without a D-id cite the section or brief id, e.g. "(G1 §15.4)", "(TU4)".
- 250-350 lines. Prefer rules that constrain what code may do over descriptions of how code works.
- Do not include orchestration process (that lives in the orchestrator's skill), rig experiments' results, or superseded pipelines (O4, N3, old router) except one line saying they are superseded and where the old code lives if it is still compiled.

## Report
The section list with line counts, and a list of (a) rows you treated as superseded (id -> by which), (b) facts you were unsure about (with the ids involved), (c) contradictions between DECISIONS and DESIGN-G1 or the code you noticed. When you have reported, stop.
