# BRIEF: E5 the simplest drawing on every hub (measurement, read-out only; D427)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: about 60 minutes in total. Each single drawing solve has a 15 s cap; each pipeline run keeps its defaults (`final_seconds = 5`). Run the hidden test once. No rerun loops, no tuning.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng1` (branch wt-e1), head HEAD_HASH, clean. Release build `/home/pete/OpenTTD/.claude/ng1/build-rel`.
- Context (D427, docs/junction-tool/DECISIONS.md): the engineer wants a working drawing for any hub before compactness. Question: which of the simplest keep-apart configurations, needing no side choices, gives a slack-0 drawing on each hub and on how many of each hub's variants. Convex cells (`clearance = true`, `pairwise = false`) decide every separation from Shape's dummies, so the drawing is one LP; they were too strict for V's tight geometry, which no longer matters. Optional flex is the other source of difficulty, so each configuration runs with flex free and with every flex insert held at 0.

## 2. Read first (once each)
- `src/tests/junction/planar/planar_draw.cpp`: `ShapeFirst` (~line 52), the hub pipeline test (~358, hub list and options), `LoadPinned197` (~1670) and how "the pipeline on PSG 197 (D422)" (~3672) builds its variant.
- `src/junction/planar/planar_draw.h`: `PlanarDrawOptions` (`clearance`, `pairwise`, `corridor`, `elastic`, `elastic_families`, `flex_zero_from`, `flex_zero_below`, `stretched`, `lengths`, `dump_path`), `PlanarDrawPipelineOptions`.
- `BuildPlanarStructure` / `PlanarStructureResult::variants`, `ShapePlanarStructureVariant`.

## 3. Change: one new hidden test "planar draw: the simplest drawing on every hub", tag `[.][planaranyhub]`. No production code change.
- Hubs T1, T2, T3 (`PlanarTText(n)`), LL2 (`PlanarLL2Text()`): `BuildPlanarStructure(topology, spec, 8)`; shape each handed-down variant with `ShapePlanarStructureVariant` (a variant that does not shape: WARN its tag and why, skip). PSG 197: the one variant the 197 pipeline test builds.
- Per shaped variant, four runs:
  - **K1 cells, flex free:** `CheckPlanarDrawable(v, 15, {.clearance = true, .corridor = true, .elastic = true})` (pairwise off; other options default).
  - **K2 cells, flex 0:** as K1 plus `flex_zero_from` = a lengths table of the variant's shape with every entry 0 (same shape as `PlanarDrawResult::lengths`: per track, per piece; build it from `v.tracks[t].pieces.size()`), `flex_zero_below = 1`.
  - **K3 pipeline:** `DrawPlanarPipeline(v, o)` with defaults (`final_seconds = 5`); only for the first variant of each hub and for 197 (time).
  - **K4 integer:** for K1 and K2 runs with total slack below 1e-6, the same options without `elastic`, 15 s: status and ms.
- WARN per run: `anyhub <hub> <tag> <K>: <status> slack <s> ms <ms>` plus one line of slack by family (`<family> <rows> <sum>`) and the 5 largest stretched rows; for K3 the last round's status and slack, the final's status and the stopped stage.
- Summary WARN per hub and K: variants with slack 0 / variants run, and for K4 variants integer Optimal.
- With env `PLANAR_DUMP=<dir>` set, K1 and K2 of each hub's first variant write `dump_path = <dir>/anyhub/<hub>-<K>.json`.

## 4. Do not
- No production change, no default change, no interpretation, no fix.

## 5. Stop conditions
- `flex_zero_from` cannot hold every insert at 0 the way §3 says: run K2 as K1 with the note "K2 not run" and say why (no stop).
- Cell mode refuses a hub (cells not refined): WARN the why, continue.
- A gate fails for a reason you did not cause and cannot isolate in one rerun.

## 6. Gates and measurement
- `cd /home/pete/OpenTTD/.claude/ng1 && tools/junction/jt fast build-rel '[planardraw]' 5`.
- `PLANAR_DUMP=build-rel/dumps build-rel/openttd_test "planar draw: the simplest drawing on every hub" > build-rel/anyhub.log 2>&1`; copy to `docs/junction-tool/wip/probes/e5land/anyhub-<short head hash before your commit>.log`, and the anyhub dumps to `docs/junction-tool/wip/probes/e5land/dumps-<same hash>/anyhub/` (skip files over 5 MB and say so), same commit.
- One commit: `Add: Junction: the simplest drawing on every hub, a measurement (E5, D427)`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. No merge or push.

## 7. Report (short, fixed format)
- Commits: hash and subject; files changed.
- Suite result.
- The summary lines verbatim; per hub the first variant's K1/K2/K3/K4 lines with families; join Catch's 80-column wraps first.
- Stops and anything left undone. State: `git status --porcelain` and `git log -1 --oneline`.

When you have reported, stop.
