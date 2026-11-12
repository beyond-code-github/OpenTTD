# LS1: level solve 4x faster with identical results, plus the sound cone reject (RECON-level-solve-cost, P1)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes2 (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes2 checkout -B wt2-ls1 <HEAD>` (given at launch). The branch wt2-f1 holds F1's WIP commit 043c159963: leave it alone. Never use git stash. Another agent works in /home/pete/OpenTTD/.claude/nodes: never touch it. Use absolute paths.
- P1 (docs/junction-tool/RECON-level-solve-cost.md, read it whole) measured over 48 real fixtures x 153 offsets (7344 placements): SolveJunctionLevels costs 8.5 ms (plans) / 12.6 ms (failures), 80% in coordinate descent, cause src/junction_levels.cpp:402-407 (per group visit, every outside neighbour restricted and propagated). Its experiment patch /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/p1-experiments.patch (on top of F1; switch `g_jl_opt`/`P1_OPT`, best = 25 = bits 1 + 8 + 16) is the reference implementation.

## Read first (once each)
CODINGSTYLE.md (C++ part); the RECON; src/junction_levels.{h,cpp}; the experiment patch (only its junction_levels.cpp parts and the `[.p1recon]` test).

## Tests (outermost first; record each red)
1. **Outer, `[realterrainsweep]` "level solve: identical plans at every near offset":** for every fixture and every offset d with |d.x| <= 8, |d.y| <= 4 around the sweep's centred offset, PlanJunctionHover; fold into one 64-bit hash (FNV-1a or the patch's) per offset: planned or not, height_cost, track_levels, terraform corners and cost; print `LEVELHASH offsets=N planned=P hash=H ms_total=T`; `CHECK(hash == <pinned>)` with the hash measured at launch HEAD before any change, and `CHECK(planned == <pinned>)`. Not red by design (characterisation): record the launch-HEAD line, then the line after each change. The why texts are not hashed (the cone reject changes them).
2. **`[hoverplan]` "the cone reject is sound":** on 2 fixtures and offsets where P1 saw a cone reject (pick from the sweep: a tile with no option, and a range that cannot meet), SolveJunctionLevels fails with the cone reason; and over the outer sweep's offsets no cone reject happens where the solve without the check plans (count `cone_rejects` and `cone_rejects_that_would_plan` = 0 in the sweep, via the check's own entry point if it has one; otherwise say how you counted). Red: before change 3 the cone reason never appears.

## Changes (each a separate measured stage, all in src/junction_levels.cpp)
1. Bit 1 (faster descent): a group's level range from its constraints to outside neighbours (all have k <= 0) instead of restricting and propagating each neighbour; identical result.
2. Bit 8 (as the patch names it; per the RECON lazy reason formatting): constraint reasons formatted only when a failure reports them; identical result.
3. Bit 16 (cone bounds): the corner cone (two-pass L1 distance transform over corners no terraform can move), per-tile option filter, reject when a tile has no option or the ranges cannot meet; placed before the corner constraints; failure reason names the tile (and corner) and says "cannot reach the ground the terraform must keep" (or the patch's wording if it names the tile). No `g_jl_opt`, no env var, no probes: the chosen code only.
- Not the Evaluate variant (finding 10).

## Stop conditions
- The outer hash or planned count differs from launch HEAD after any change (report which stage); `cone_rejects_that_would_plan` > 0; anything not covered.

## Gates
- `tools/junction/jt fast build '[hoverplan]' 5`; `build-rel/openttd_test "[realterrainsweep]"` (HOVERPLAN line unchanged apart from ms; report times before/after and the new test's time); `"[topoprojsweep]"` ridges unchanged; `tools/junction/jt gate build-rel` (report the count). TESTS.md rows.
- One commit: `Change: Junction level solve 4x faster with identical results, and rejects early when the kept ground cannot be reached`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus LEVELHASH lines per stage and the level-solve ms before/after. When you have reported, stop.
