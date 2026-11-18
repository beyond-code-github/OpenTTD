# S2b land: side rule only (drop the angle cost), measure against saved before runs, commit

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes; build only `openttd_test` in `build-rel`. ONE heavy job at a time (build, one pv run, or one gate run; the machine has a known CPU instability). Scratch: `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/S2bl/`.

## State
Worktree `/home/pete/OpenTTD/.claude/ng1` at d632dc357c with S2b UNCOMMITTED (= `/home/pete/OpenTTD/docs/junction-tool/wip/S2b-wip.patch`). Read `wip/BRIEF-S2b-shape-rules.md` and `wip/DIAG-S2b-loss.md`: the side rule loses nothing (cost 0 reproduces every pre-S2b program result); the angle cost (`SHAPE_CROSSING_ANGLE_COST = 1`) turns T2's winning 45° crossing (X1, F1 under B1) into 90° and loses 862.25 -> 906.25. Ruling (orchestrator, reversible): no angle opinion in the shape program; crossing angles are left to the length solve's scoring of shapes.

## Changes
1. Remove the angle cost entirely from `planar_shape.{h,cpp}` (the constant, its u/v columns and objective terms, its doc comment) and its CLAUDE.md line; the side rule and everything else of S2b stay.
2. Tests: unchanged S2b tests must pass; `[planarshape]` incl. slow; T2/T3 mapped slow tests (the T2 862.25 test must pass); `jt gate` once (report time; if a test fails, rerun once and report both).

## Measure (only "after": the "before" runs are saved)
Before files (d632dc357c binary, default mode): `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/S2bf/default-T2-before.txt`, `default-T3-before.txt`, `default-LL2-before.txt` (LL² with `--tags /home/pete/OpenTTD/docs/junction-tool/wip/ll2-tags.txt`, keep that exact file). Runs, one at a time, `-j 6`:
- after, default mode: T2, T3, LL² -> `pv diff` against the before files. Safety question: no variant's area worse, none newly failing.
- after, `PLANAR_SHAPE=program`, LL² only: count failing variants by reason (before S2b: 64 failing, 32 'infeasible with topology sides'). Fix question: are the side failures gone?
Report per hub: solved, best area, better/same/worse, median/max ms.
Stop before committing if a default-mode variant gets worse or newly fails.

## After committing
Save durable baselines: copy the three after-default pv files to `/home/pete/OpenTTD/docs/junction-tool/wip/pv-baselines/{T2,T3,LL2}.txt` with a one-line `README` there naming the commit and the LL² tag file. Do not commit in the main checkout (the orchestrator does).

Commit: `Change: Junction shape program keeps every crossing's side` + body (tables) + `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Report
Commit; gate; tables; LL² program-mode failure counts by reason; stops; `git status --porcelain`, `git log -1`. Under 15 lines. When you have reported, stop.
