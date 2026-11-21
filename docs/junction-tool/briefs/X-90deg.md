# Brief X-90: pieces join only through moves a train can make (no 90-degree turns)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-x`, head `{HEAD}`. Python only, `python3 -I`.
- Finding (X-cyc, 9aaebb157b): the extractor's line graph has cycles. 197 has 8 lines on cycles (L98/L113 at 109,171; L74/L77 at 100,182/101,181; L78/L83 at 101,183/102,182; L86/L87 at 102,183/103,182). Brainmelter has 9 (around 214-219, 265-273). L74 turns a full 360 degrees in about 7 half tiles (`lengths L74: 1 1 3 1 1`), and L98 is five pieces of length 1. These are tiny rings of diagonal corner pieces around a tile corner, which no train can drive. Each step round such a ring is a 90-degree turn, which OpenTTD forbids when `forbid_90_deg` is set (coop servers set it).
- Cause: `load.py` `World.across(pc, e)` joins every piece of the neighbouring tile that has the opposite edge, including a perpendicular diagonal: Upper or Lower meeting Left or Right across an edge is a 90-degree turn.

## 2. Read first (once each)
- `tools/junction/corpus/README.md`; `load.py` (`World`, `across`, piece names in `nodes`, `build_dirs`); `extract.py` `find_cycle` and where the graph stage uses `across`.
- OpenTTD's rule, for reference only: `src/track_func.h` `TrackCrossesTracks` / `TrackdirCrossesTrackdirs` (a diagonal track crosses the two diagonals of the other orientation), and how the pathfinders mask next trackdirs with `forbid_90_deg` (`src/pathfinder/yapf/yapf_costrail.hpp` or `follow_track.hpp`, search `forbid_90_deg`).

## 3. The rule
- In `across`, when `forbid_90_deg` is in force, a diagonal piece (Upper, Lower, Left, Right) does not join a diagonal piece of the other orientation across an edge. Upper and Lower are one orientation, Left and Right the other: Upper/Lower to Left/Right, either way, is a 90-degree move. Straight pieces (X, Y), and same-orientation diagonals, join as today. Tunnel and bridge heads are as today.
- `forbid_90_deg`: read it from the save export if the export carries game settings (search the export JSON's top-level keys). Otherwise take it as true and note "forbid_90_deg assumed (not in export)" in the report. A hub id may not override it; there is no special case.
- If the piece names in the export are not Upper/Lower/Left/Right/X/Y (or the pairing differs from OpenTTD's `TRACK_UPPER`... `TRACK_RIGHT`), stop and report the names you see.

## 4. Checks
- psg197 and psg186: `find_cycle` reports `cycle: none`, and `topology.txt` is written again.
- psg197: `compare.py psg197`; record the differences before (from c9e93b35c4's compare.txt) and after. A change anywhere other than the ring lines is a finding: list it.
- psg197 report: `ambiguous pieces` and `off-route` counts before and after.
- All 16 pinned lanes of psg197 still agree (no "differs from the save's own signal direction" note).
- A one-off `python3 -I` walk (not committed): the largest number of paths per route over the new psg197 topology.

## 5. Do not
- Change lane directions, `hubs.tsv`, the cycle guard, or the hand-authored topologies.

## 6. Commit
- `Fix: Junction corpus: pieces join only through moves a train can make (forbid_90_deg)`, regenerate and commit `out/psg197` and `out/psg186`, ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Do not commit `__pycache__`. README: one sentence on the rule.

## 7. Report (short, the full content, no placeholder)
- References read; commit; forbid_90_deg source (export or assumed)
- Cycles before/after (197, 186); compare.py before/after; ambiguous/off-route counts; pinned lanes; path counts
- Stops; state (`git status --porcelain`, `git log -1 --format='%h %s'`)

When you have reported, stop.
