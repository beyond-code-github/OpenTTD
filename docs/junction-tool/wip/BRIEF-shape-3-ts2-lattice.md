# BRIEF shape-3 (probe): is TS2 drawable at one-lane spacing when S-bends are offered inside the crossing lattice?

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 45 minutes wall; each solve capped at 15 s; no rerun loops. Probe only: extend the hidden `[.][shapeprobe]` test and the notes; no production change except, if needed, a probe flag that is OFF by default (name it in the report).

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- TS2 at the default (one-lane) port spacing is Infeasible with 90-degree crossings (ts2-diag.md) and with 45-degree crossings (shape-2, `docs/junction-tool/wip/probes/shape/ts2-angle.md`: conflict = the three crossing rows + the one-tile N gaps). Reading: the lanes need S-bends to spread before they cross, and Shape's flex placement offers none between crossings of one channel (the lattice clause, `planar_shape_layer.cpp` ~963, `FreeOnePieceStretch`; recon-steps.md).
- Question: with S-bend offers on EVERY stretch (the lattice clause off; flex C otherwise), does TS2 draw at the default spacing? With 90-degree and with 45-degree crossings.

## 2. Steps
1. Find how to place flex with the lattice clause off: an existing option, else add `bool lattice_flex = false` to whatever options struct `PlacePlanarShapeFlex` takes (or a file-static test hook if it takes none — say which), doc "A probe (shape-3): offer flex on stretches between crossings of one channel too (the lattice clause off)".
2. TS2 (Shape's variant and the 45-degree variant from shape-2), flex placed with the lattice clause off, drawn at the default spacing with the DrawnFixture options, inserts free (may be 0), `least_extra = 0`: status, ms, size, the number of inserts used (> 0); if Optimal: the domain's verdict on the points, and which inserts were used (track, piece, length).
3. If still Infeasible: the conflict by deletion over row families (at most 30 solves).

## 3. Output
Append to `docs/junction-tool/wip/probes/shape/ts2-angle.md` a section "lattice flex"; commit `Doc: Junction: TS2 with S-bends offered inside the lattice (probe)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 4. Report (short)
References read; commit; the flag added (or none); both solves; the used inserts; the domain verdict; the reading in two lines; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
