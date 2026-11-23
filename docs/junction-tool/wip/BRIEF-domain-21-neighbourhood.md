# BRIEF domain-21: generic neighbourhood configurations

> You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 75 minutes wall. Test code only (no gate). One Release run per fixture, rule set `span+lead+endnode+crosspiece+grid`. Record findings; fix no rule. Never run git stash. Do not touch branch wip-defaults.

## 1. State and why
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-e1`, head `<HEAD>`. Commit there; do not merge.
- Hand-named configurations keep coming out thin or degenerate: M1c/M1d (a pinned leg), L1a (1 polyline), Y1 (1 polyline: the base drawing is compact and the named pieces had no room). Replace them with one generic rule that needs no piece names, so every fixture, existing and future, gets a dense sample of real geometry near its events.

## 2. The generic configuration (harness)
`NeighbourhoodConfigs(fixture, k)`: for each event of the base (each node point and each crossing), and for each track touching it, the k pieces of that track nearest the event (on its side(s) of the event) are varied:
- a flex insert or a contracted S-bend insert: in {0, 1, 2} (diagonal) or {0, 2, 4} (axis);
- a flank or run piece: base length plus {-4, -2, 0, +2, +4} (axis) or {-2, -1, 0, +1, +2} (diagonal), clipped at 0;
- the track's nearest piece of the same heading OUTSIDE the varied window absorbs, so the track's far end (the next event beyond the window, or its port end) stays where the base has it; a geometry needing a negative absorber is skipped;
- one configuration per (event, track), named `<event> <track>`; tracks vary one at a time (other tracks at base), plus one configuration per crossing varying both of its tracks together.
Through the on-grid generator; off-grid geometries judged with `+grid` as now. k = 3.

## 3. Use it
- Every fixture (X1, X2, M1, K1, L1, Y1) gets its neighbourhood configurations IN ADDITION to its existing ones (keep them; the generic ones are new sections). Report per fixture the total polylines judged (it must be well above the hand-named totals for M1, L1, Y1; say so if not).
- Report per fixture the lead-row and corridor-row counts as now.

## 3a. Test (fast)
"planar domain: neighbourhood configurations cover X1's events": X1 gives one configuration per (event, track) plus one per crossing, and each keeps its tracks' far ends at base.

## 4. Output
- `tools/junction/jt fast build '[planardomain]' 5` (under 1 s Debug).
- Release once per fixture (X1, X2, M1, K1, L1, Y1), `span+lead+endnode+crosspiece+grid`, overwrite the reports; timeout 300 each.
- One commit: `Add: Junction: generic neighbourhood configurations in the domain suite (D428)`, Co-Authored-By Claude Sonnet 5.5.

## 5. Report (short)
Commit; per fixture: polylines judged (hand-named vs neighbourhood), classes with group keys, counts and the first example of every non-zero too strict or too loose group; lead and corridor counts; wall times; state. When you have reported, stop.
