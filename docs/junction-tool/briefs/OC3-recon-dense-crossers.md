# OC3 (recon): dense crossers on a doubled lane (LL_RR x LL_RR per direction: "bridge heads hug crossers" contradiction; OC1 §3)

Read-mostly recon: you may add temporary prints/tests; commit nothing; save any experiment as a patch in /tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/oc3.patch and leave the worktree clean. Keep your context small. Never use git stash. Never start a game client or touch the engineer's game or rig servers. Mark anything not read or measured as INFERRED.

## State
- Worktree /home/pete/OpenTTD/.claude/nodes (Debug `build`, Release `build-rel`). First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-oc3 <HEAD>` (given at launch). Other agents work in /home/pete/OpenTTD/.claude/nodes2 and .claude/ng1: never touch them. Use absolute paths.
- OC1 (docs/junction-tool/RECON-nonadjacent-crossing.md §3): under pairing per direction, LL_RR x LL_RR passes order/structure/programs and fails placement with a cycle: hug crossers (e14) <- lane split run (e14) <- hug crossers (e15) <- lane split run (e15) <- chain runs of e10, e2 <- S port spacing <- chain run of e3 <- bridge heads (e3) <- hug crossers (e14). INFERRED geometry: the S ports pin 5 crossers into ~4 columns while the doubled E->W lanes' bridge heads must hug them, stay a floor beyond the lane-split nodes and keep the D118 stagger. D203 (rows may spread inside the hub; one bridge may straddle several crossings), D209 (series bridges, cap ~2 crossers), D118 (stagger), DESIGN-G1 §11.9 (series bridges, cut).
- MJ1/OF1/OF2 merged since OC1; re-measure.

## Questions
1. Re-run per direction at HEAD: the reason chain in full, and the solved/least columns of the crossers, the S ports and the heads (a dump): confirm or correct OC1's geometry.
2. Which relation forces the crossers into so few columns (S port spacing? the S trunks' straight runs? the order?), and what general relation would let them spread (e.g. an S-bend on a branch trunk between the S port and the crossing — a token the programs do not emit today?; a longer S port spacing? the bridge cut choosing a series split, D209?). Prior art in programs/placement. Say whether a cut choice (CutJunctionTrackBridges) already offers a feasible split and why it is not taken.
3. The plainest general change, which stages it touches, and whether it needs an engineer ruling (a card if so).

## Output
docs/junction-tool/RECON-dense-crossers.md (untracked, ~50 lines) and the patch path if any.

## Report
10 lines: the confirmed geometry, the forcing relation, the proposed general change, any decision for the engineer. `git -C /home/pete/OpenTTD/.claude/nodes status --porcelain` (clean). When you have reported, stop.
