# BRIEF domain-51: a line running along a tunnel, as a behaviour (D438, step 1 of 2)

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 60 minutes wall. Test code only (the implementation is step 2, after I read your recon). Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- D438 (engineer, 2026-11-23): a crosser may cross a tunnel with its heading equal to the separator's (mod 4) over the tunnel's middle tiles: V's L40 over L57's tunnel and L89 over L74's on PSG 197 (an S-bend whose straight middle runs directly above the tunnel). Tunnels only.
- Read first: DECISIONS D438, D376, D436; `docs/junction-tool/wip/probes/domain/triage-197-judge.md` §3 (the L40/L57 and L89/L74 rows and tiles); the X1 fixture (hand-placed: its `shape` text and `starts`, how `LoadPlanarShapeVariant` takes it) and the BR1/TT1 fixtures; `planar_domain.cpp`'s structure middles (what the domain allows on a tunnel middle tile); in the shape layer, where the crossing angle 1..3 eighths is checked (grep `angle` / `H4` / `H5` in planar_shape_layer.cpp).

## 2. Changes
1. Fixture XA1, hand-placed (like X1, NOT through Shape's heading solve): X1's topology and `tunnel T1 A1: B1` structure, with B1's shape text an S-bend whose middle runs along A1 above the tunnel: B1 heads north, turns to east (A1's heading) over A1's tunnel middle for its crossing, then turns north again (headings e.g. `0 1 2 1 0` on B1's crossing stretch; choose the headings and starts so B1's middle piece lies on A1's line over A1's middle tiles and both ports stay on the box edge). If `LoadPlanarShapeVariant` (or a Shape check it runs) refuses the angle-0 crossing, record its message: that refusal is part of what step 2 changes. Then build the variant without that check only if the loader has a documented way to skip it; otherwise stop and report.
2. Behaviour tests (through the judge):
   - "planar domain: a line may run along a tunnel's line above its middle (D438)": XA1's base: the domain accepts (assert, with the tiles if not), the judge accepts: `[!shouldfail]` if the judge refuses, with a comment naming D438 step 2.
   - Counterpart (normal test): the same run along a BRIDGE (BR1-shaped, `bridge B1 A1: B1`): the domain refuses (a deck is the surface) and the judge refuses.
   - Counterpart (normal test): B1's along-run extending past the tunnel's portal onto A1's open track: domain and judge refuse.
3. Recon for step 2 (no change): for XA1's base, list every row the drawing program writes that refuses it (tag, family, why), and every `unsupported` status or Shape check that blocks it, with file:line. This is the list step 2 implements against.

## 3. Gates and commit
- `tools/junction/jt fast build '[planardomain]' 5` (Debug).
- One commit: `Test: Junction: a line running along a tunnel as a behaviour (known failure) (D438)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`; the recon in `docs/junction-tool/wip/probes/domain/d438-recon.md` in the same commit.

## 4. Report (short)
References read; commit; XA1's shape text and starts; each test's verdicts; the recon list; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
