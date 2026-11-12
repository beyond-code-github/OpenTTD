# C5b: the prio element's footprint as an occupancy family; the signal pass fails loudly (RECON-prio-stretch; D217)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git checkout -B wt-c5b <HEAD>` (given at launch). Never use git stash. Another agent may work in .claude/nodes2: never touch it.
- RECON-prio-stretch.md (read it whole) and its checked patch /home/pete/OpenTTD/docs/junction-tool/wip/RECON-prio-stretch.proposal.patch (made at 841b1fe4ac, before R2-R5: re-apply by hand to the current code; the occupancy enumeration now reads chains). The prio element's row and joins are drawn by the signal pass and are not occupants, so a row stretch can put another track on the row's step 0; SignalJunctionTrackRoutes phase 0 then silently skips the element (`if (!fits) continue;`) and the checker reports Prio.

## Changes
1. The recon's footprint occupants in the occupancy family (every main-main row whose joiner-merge node has the X axis plus one side half; step 0 with h2 bits, steps 1..even_prio with X bits, on the row side opposite the joiner's approach; related to their own node, never grouped or tied; on equal columns first in travel direction).
2. The signal pass no longer skips silently: an element that does not fit is a failure the caller sees (SignalJunctionTrackRoutes returns its tokens plus a `why` out-parameter, or the existing error path if one exists; read its signature and callers and use the existing pattern; if there is none, STOP and report).
3. `[topologystretch]`'s Y part REQUIREs rule 9 clean too (now every rule).

## Tests (record each red)
- `[topologystretch]` with rule 9 required: red before (the 3 runs), green after.
- `[topology]` "topology: the signal pass names a prio element that does not fit": a hand-made placement (edit a seed's point so another track sits on a row's step 0) makes the signal pass report it. Red: it skips silently.
- Every existing test and sweep green, seeds identical, circuit byte-identical.

## Gates
- `tools/junction/jt fast build '[topology]' 5`; `'[topoproj]' 5`; `build-rel/openttd_test` with `"[topologysweep]"`, `"[topologystretch]"`, `"[placeoracle]"`, `"[realterrainsweep]"`, `"[topoprojsweep]"` (to files); circuit byte-identical; `tools/junction/jt gate build-rel` (report the count).
- One commit: `Add: Junction prio element footprint in the occupancy family; the signal pass reports an element that does not fit`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
The standard report. When you have reported, stop.
