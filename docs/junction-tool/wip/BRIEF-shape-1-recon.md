# BRIEF shape-1 (read-only recon): Shape's step model against V's 7 tracks and TS2

> You execute this brief and nothing else. Read-only: no code change, no commit except the notes file. If something is not covered, stop and report. Keep your context small: read with offset/limit, send command output to files.

Budget: 45 minutes wall.

## 1. Why
Before Slots (D441), Shape must be able to hand over V's PSG 197 and must not hand over undrawable headings (layers outside-in, D368). Two findings:
- Too strict: Shape's flexed variant cannot carry 7 of V's tracks (domain-36): L3 `0 1 0 -1 0 -1 0` and L85 `2 1 2 1 2 3 2` (two steps in one stretch), L38 `5 4 5 4`, L44 `3 4 3 4`, L72 `7 6 5 6 5 6`, L77 `6 5 6 5` (staircases), L89 `5 6 5` (a step on a stretch Shape makes one piece). Source: `docs/junction-tool/wip/probes/domain/answerkey-197.txt` "Shape findings" and `tools/junction/corpus/out/psg197/steps.txt`, `shape.txt`.
- Too loose: TS2 (three lanes, `docs/junction-tool/wip/probes/domain/ts2-diag.md`): Shape's headings cross lane 2 at 90 degrees from lane 3's diverging leg, which the default one-lane port spacing cannot hold.

## 2. Read (cite file:line for every claim)
- DECISIONS D395, D396, D402, D413, D360/D362/D383 if present, D400 (node rule), D376, D368; `docs/junction-tool/DESIGN-sss-v2.md` §3.2; `docs/junction-tool/ROADMAP.md` lines about Shape; SPEC-shape if it exists (find it).
- `src/junction/planar/planar_shape_layer.cpp/.h`: the heading program (H1-H11 constraints), `PlacePlanarShapeFlex` (where flex pairs go: D402's rule), what `steps` are and who sets them.
- `src/junction/planar/CLAUDE.md` Shape bullets.

## 3. Report in `docs/junction-tool/wip/probes/shape/recon-steps.md`
1. Shape's model in ten lines: what a stretch is; what Shape decides (headings per stretch, monotone); where flex pairs are placed (D402) and why one per stretch; what a step is (D395) and when Shape emits one; what decides a crossing's angle.
2. For each of V's 7 tracks: its stretch (between which events/nodes), Shape's projection, the flex pairs Shape places there, and which specific rule prevents V's raw headings (one flex pair per stretch? monotone projection? the single-piece stretch has no flex?).
3. For TS2: which Shape constraint chose the 90-degree crossing of lane 2 from lane 3's diverging leg; what Shape knows about port spacing (does any H-row read lane spacing or the side gap?).
4. The existing decisions that would have to change for each (e.g. D402's "one flex pair per free stretch", D413's "flex on every piece" trial and why it was not adopted).
Commit only that file: `Doc: Junction: Shape's step model against V's 7 tracks and TS2 (recon)`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Report: commit, and the ten-line model plus one line per track/TS2. When you have reported, stop.
