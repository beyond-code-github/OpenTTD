# TS1: a branch trunk may S-bend between its port and a doubled bridge it passes under (RECON-dense-crossers.md; D203)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-ts1 <HEAD>` (given at launch). Never use git stash. Other agents work in /home/pete/OpenTTD/.claude/nodes2 (near reject) and .claude/ng1 (tunnel levels): never touch them. Never start a game client or touch the engineer's game or rig servers. Use absolute paths.
- OC3 (docs/junction-tool/RECON-dense-crossers.md, read it whole; patch /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/oc3.patch has dumps and ablation globals: do not commit those): LL_RR x LL_RR (per direction) fails placement in a positive cycle because each S trunk's column equals its port column (chain run 0) with no token to step it sideways before a doubled bridge; releasing the chain run on the trunks that the cycle names places (68 x 21 in the ablation; render then fails without real tokens). The programs stage already emits an SBend before the Under pair for a SINGLE bridge; placement has the chain element, the free S-bend size relation and D113; render handles S-bends. D203 (rows may spread inside the hub) covers S-bends inside the box (OC3's reading; recorded for the engineer as FYI).

## Changes
1. Programs: a branch trunk edge that passes under a DOUBLED bridge emits an SBend token before its Under pair, as it already does for a single bridge (same code path, same hand rule; say which hand and why). Its size is a placement variable whose least value keeps today's boxes: if the S-bend element cannot have size 0 (a zero S-bend), STOP and report (identity would break for every seed with a trunk under a doubled bridge).
2. Nothing else.

## Tests (record each red)
1. Outer identity: `tools/junction/jt identity build-rel`; the five sweep tags' outputs equal to launch HEAD's (times stripped); LL_RR circuit byte-identical. If any seed changes, STOP and report which.
2. `[topologysweep]` (extend MJ1's LL_RR x LL_RR test): under pairing per direction the seed now places; render + signals + CheckJunctionRules: clean or the first violation (report it with tile and rule; a violation is a STOP, not a fix here); box size, seed ms. Red: today's placement contradiction.
3. NM1 rerun (local patch /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/of2/nm1-local.patch; never committed): 729 runs, 0 crashes/timeouts; counts per pairing vs MJ1's (OK count per pairing is the headline).
4. TESTS.md row.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `tools/junction/jt identity build-rel`; `tools/junction/jt gate build-rel` (count, time).
- One commit: `Change: Junction branch trunks may S-bend before passing under a doubled bridge`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report, plus LL_RR x LL_RR per direction's result and the NM1 OK counts. When you have reported, stop.

## Amendment 1 (orchestrator, after the stop: an S-bend cannot have size 0)
Option 1, as a per-token minimum: an SBend token may be OPTIONAL, with least size 0. Add to the program step the smallest change that carries it (a field on the SBend step, e.g. `uint8_t min_m = 1; ///< 0: an optional S-bend that may vanish (a straight run) when the placement does not need it.` — name it and say where), and in placement an S-bend element whose solved m is 0 emits no tiles and no curves: it is a straight run between its neighbours, and every reader that assumes m >= 1 (refresh, bend_tiles, the curve-gap and floor families, occupancy, render, D113) treats it as absent. Every existing token keeps min_m = 1 (so every seed is identical). The new trunk S-bend before a doubled bridge's Under pair is emitted with min_m = 0.
Step 0 for this amendment: list every reader of an S-bend element's m or its curves (file:line) and how each treats m = 0; if one cannot (e.g. a relation that needs the two curves to exist), stop and report it.
Then the changes, tests and gates as the brief says.
