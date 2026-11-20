# BRIEF E1j: PSG 197's six gated merges: prio or lane choice? (recon, data only)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Repo /home/pete/OpenTTD, branch feature/junction-tool, head HEAD_HASH (check `git log -1`). Main checkout, data only: no build, no tests, one Python process at a time. Budget 40 minutes.
- Question (engineer, 2026-11-20): "is it even a prio in 197? Or just presignals for lane choice?" psg197.txt's `prio` line rests on one rule (gaps §9): a merge with an entry signal in the last 8 pieces of one leg. Six merges: 86,151r (C23 gated), 98,171l (C2b gated), 107,173x (D2b gated), 100,174d (C2 gated), 110,176x (D2 gated), 94,195u (A2c gated). Gaps §3: 25 of 50 tunnel entrances carry an exit pre-signal 1-3 tiles before the head, fed by an entry signal on the line before a split (choice by free tunnel).

## Read first
- docs/junction-tool/wip/topologies/psg197-gaps.md §3 and §9.
- docs/junction-tool/tools/sigsim.py (a port of OpenTTD's signal.cpp segment exploration and pre-signal propagation; read its segment exploration and pre-signal logic; it reads a layout JSON, so port the logic you need onto the trace graph rather than converting the save).
- The trace, OD = /mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/psg/anatomy/psg197/trace: core.py (`build()`: pieces, successors, `N.sig` with signal types and exit edges), signals_bridges.txt (head only). Run trace code with `python3 -I`, imported by importlib as core.py does; do not modify anything under OD.

## What to find, per gated merge (facts, not a verdict)
For the entry signal on the gated leg (the one gaps §9 counted):
1. Its tile, piece, type and the direction it faces.
2. The signal block it protects: every (tile, piece) reachable forward from it without passing another signal, by the segment exploration of sigsim.py (OpenTTD's rule: a signal tile belongs to both segments it bounds).
3. Every signal bounding that block, with tile, type (entry, exit, combo, block, path) and whether it faces out of the block (a train leaving the block passes it) or into it.
4. For each exit or combo signal facing out of the block (the ones that decide the entry's aspect): where it is relative to the merge: on the merge's other leg (upstream of the merge), on the merged line past the merge, or within 1-3 tiles of a tunnel entrance (name the tunnel number from psg197-decisions.md's tunnel table).
5. Whether the other (holding) leg has any pre-signal (entry, exit, combo) within its last 8 pieces before the merge.

## Output
docs/junction-tool/wip/probes/e1/gated-merges.py and gated-merges-out.txt (per merge items 1-5), and a short table at the top of gated-merges-out.txt: merge, the block's exits by place (other leg / past merge / at tunnel T<n>), holding leg's pre-signals.

## Commit
`Doc: Junction E1j: PSG 197's six gated merges, the pre-signal blocks traced`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`; only the two files. Do not push.

## Report (short)
References read; commit; the table; anything you could not trace and why; `git status --porcelain`, `git log -1 --oneline`. When you have reported, stop.
