# RECON: mergers (ABR06 styles) vs today's lane merges

Read-only recon, 2026-11-13, from commit 92560e549c. INFERRED marks my reading. Line numbers are today's files.
Source text: ABR06 "Mergers" is at `/mnt/c/Users/micro/OneDrive/Documents/OpenTTD-junction-tool/scratchpad/abr/06-advanced-building-revue-06-hubs.txt` lines 54-77 (styles) and 94-98 (Merger Logics). Junctionary pages "Line Merge" and "Priority Merge" are in `junctionary-mirror/pages/` (two lines to one; priority = give one line absolute way). Summary of the styles: RULES-hubs.md H23-H26, H38 (lines 52-54, 77).

## 1. Today's D224 band merge (LL_RR x LL_RR, any pairing)

Build (graph)
- `MakeJunctionHubTopology` (junction_topology.cpp:1232) makes lane paths per movement. Pairing AnyToAny (default, D221) gives a path from every branch-lane to every main-lane (`paths` loop, junction_topology.cpp:~60-110); LaneKeepsLane / LanePerDirection give one-to-one or dealt lanes and so no band merge.
- The merge-tree builder (struct at junction_topology.cpp:234) makes one merge node per main-lane target. For a main lane's merge node whose out edge carries only branch paths ("branch_only", :397-404), the node level is `2*row+2`: the band below the lane. That is where the D224 merge sits: the legs from each branch lane meet in that band node, and one edge (the joiner) goes up to the main lane's row node. Exit side mirror: :405-413 (splits among branch lanes in the band below the lane; only the first split of a cascade is on the row, DESIGN-G2 G2.2).
- MJ1 (:607-690): a merge node entered from its legs needs those legs adjacent items; fails with "the legs of a merge cannot be brought together" (:659). The band bundle is ordered with `bands[level/2]` items (:679).
- Programs: the legs reach the main lane through a Fan step (:1121-1124: `StepKind::Fan` onto the lane/row item) and, when the lane carries a doubled bridge, an SBend LaneMerge/LaneSplit pair (:1111-1114, D229 order merge-then-bridge).

Signals (D211, DESIGN-G1 section 12; code src/junction/signals/junction_signals.cpp)
- Every route gets per-event anchors: BeforeMerge and AfterMerge at each non-bridge, non-split event (junction_signals.cpp:296-297). The band merge is such an event on a branch->main route, so it gets a plain BeforeMerge and AfterMerge signal (type = spec.signal_type, block/path per spec).
- Bays (phase 2, :303-346): on every non-through route, before every merge, whole-train bays (2*TL units, D83/D88/D176) are signalled upstream of the merge signal. Both legs of the band merge therefore get waiting bays: the queue is held on each leg before the merge, nothing else.
- Prio element (phase 0, :136-250; DESIGN-G1 section 12.4; D211 exception): it is built only for a route whose movement is main to main (:143-145 `continue` otherwise), at an ML merge where a different movement's route has an edge ending at the same node (the joiner search, :150-165). It then builds the row track, Combo/Exit signals, and turns the joiner's last signal before the merge into an Entry (phase 4, :359-367). So the prio is at the main-lane merge node (the row node), with the band merge's out edge as the joiner. The band merge itself has no prio and no Entry/Combo: its two legs queue equally behind BeforeMerge signals.
- Joiner search takes "the first route of another movement" ending at the node (:152-163): one joiner per merge node. With D224 the joiner is the single band-merge edge, which is why D224 was needed (INFERRED: two joiners on one lane would have one without a prio, DECISIONS D224 "Not two joiners in sequence").

Answer: plain merge with block/path signals plus whole-train bays (D83/D176); no priority and no pre-signal choice logic at the band merge. In ABR06 terms it is the "Not balancing merge" for the pair of legs (equal, first-come, relies on luck; ABR06 line 56-57), feeding a "Fully prioritized, 1 to N" style joiner onto the main lane (H24 SLH style; ABR06 58-63). INFERRED: the merge of branch legs from different lanes is "balancing" only in the sense that the earlier any-to-any split gave every lane access to every target (D221 A), which ABR06 would call all-to-all upstream plus a plain merge downstream.

## 2. Every place N lanes become fewer (today)

| Kind | Where built | Decisions |
|---|---|---|
| Joiner merge onto a main lane (branch joiner + ML lane) | main-lane merge node on row level `2*row+1`; prio element at junction_signals.cpp:136+ | D25, D75, D107, D159 (main-lane merges always have a prio), D119 |
| Branch legs merging before a main lane (band merge) | level `2*row+2`, junction_topology.cpp:397-404 | D224, D229 (bridge order) |
| Main-lane exit legs merging before a branch out port (lane legs of one direction merge in the outer band by the S port) | merge tree, outer band; programs LaneMerge S-bends :1111, :1127 | D187, D205 (D210 keeps it) |
| Two directions merging before the branch port | merge trees at the port | D204 (mirror), D187 |
| Main-to-main turning movements (e.g. W to N, LL to LL, pairing any): lane legs merging at the target lane | same merge-node builder as above (INFERRED: group_of(...).main on both sides; the same level rule applies) | D221 |
| Doubled-bridge copies merging back (lane merge after a bridge) | row in/out edge programs, junction_topology.cpp:1105-1130 (SBend LaneMerge/LaneSplit) | D19, D80, D100, D229; copies of one lane to one destination, not a D24 breach |
| Cascades of two-way nodes (split then split, never merge before split) | DESIGN-G2 G2.1, G2.2 | D228, D24 |
| Through main lanes | 1:1, no merge (`in.lanes != out.lanes` fails, :70) | -- |

Only the first two are traffic merges where a queue forms behind a choice; the exit merges (rows 3-4) are the "join" kind too but D187 ruled them plain.

## 3. H24 styles in our terms (shapes only)

Notation: legs = branch/exit lane legs; target = main lane row node; band = row `2*r+2` below the lane; a prio element = DESIGN-G1 12.4 (Prio item beside the lane, joiner Entry, ML signals inside coverage).

1. Not-balancing. Pairing B (lane keeps lane, D221) or any plain merge with no prio element and no lane choice: one merge node per target, legs equal, block signals + bays. Exists today as the band merge. Reuses D224 band merge; nothing new.
2. Fully prioritized 1 to N (SLH). One joiner edge per target main lane, prio element on each target; each branch lane (or the single branch lane in L_R) fans to N lanes before. This is today's L_R joiner (D186) and today's LL_RR joiner after the D224 band merge. Reuses the prio element directly.
3. Multi-to-multi (2-line joins). Two lines each reach both targets: two merge nodes, each with two incoming legs from different lines, all crossing in a short fan; equal priority, "sensitive signal gaps" (H25). In the graph: two band-level merge nodes with the legs crossing (a lane split per leg then lane merge per target), no prio element, signals only at events. INFERRED: needs the merge nodes in one band with adjacent leg ordering; MJ1 (:607) fails on crossed legs today. Reuses D224 band merge, twice, plus a crossing; bridges only if legs cross a through lane (doubled bridges, D229 order).
4. Fully prioritized some lines. Several lines join more lines; each target lane gets a prio element for only some of its feeding legs (the other legs plain-merge in the band first, as D224). Shape: band merge for the legs that share a target, then prio at the target. This is D224 plus the prio element generalised to say which leg is the joiner. Needs the joiner search (junction_signals.cpp:150) to accept more than one different-movement edge into a node (today: first match wins).
5. Half prioritized. Two equal lines; one is prioritised at one merge, the other at its mirror merge (ABR06: only one tunnel prioritised). Shape: two merge nodes (the multi-to-multi layout of 3), each with a prio element where the prioritised leg is the other one at the second node. Prio element reused with a per-node choice of joiner; the joiner needs an Entry, the prioritised leg has the ML-style signals. A prio that gives up after a while (ABR06: "stuck too long") is not expressible; INFERRED that a short coverage (D137 sizing, prio_tiles) plays that role.
6. All-to-all. Systematic fan of diagonal crossovers (`hubs_A2A.png`); N legs reach M targets through a cascade of two-way merge/split nodes (DESIGN-G2 G2.1 cascades, D228 "split then split as long as splits come before merges"). Optional prios (full or half) on the final nodes, PF traps behind the merge improve balance (D132). Reuses cascades and band merges; each stage is a two-way node; D24 holds when every path splits before it merges (legs split first by lane, then merge).

## 4. Constraints and genuinely open questions

Constraints (all grep-checked in DECISIONS.md)
- D24: splits before merges on every path. D228: cascades of two-way nodes fine, no crosser cap; D228a flow bound on doubled bridges.
- D25, D75, D87: a prio is a parallel track to the main line, joiner opposite. D107/D116: prio must pass G9b (real pre-signal logic), thin signals are a preference. D108: no lane merge or split inside a prio's coverage (G9c). So a band merge must sit at least coverage + one segment before the main-lane merge (the D107 "margin after the lane merge" idea), which any multi-node merger style also has to respect.
- D119: ML delay strictly zero; D159: main-line merges always carry a prio; a branch merge carries one only if the throughput estimate says so (so a branch-only band merge is "no prio" by D159 until the throughput model exists).
- D71, D72, D83, D88, D176: waiting bays before every merge where trains choose; whole-train legs after a line-choice split.
- D126: multi-lane to LLLL_RRRR with "line choice, balancing, optimisation"; D129: modular balance (each line must have balance over the platforms it reaches, not every permutation); D132: PF traps as balancing primitive; D221: pairing is an option (A any, B keeps lane, C per direction), A the default (reversible).
- D187, D204, D205: exit legs merge before the port, in the outer band; D210: kept, may become a cost candidate.
- D224: legs merge with each other, one joiner, reversible, "provisional" per STATUS.md line 2 (Pete: 2 to 1 needs proper mergers, ABR06, H23-H25). D229: merge vs crossing order is a cost choice.

Genuinely open (for the engineer)
1. Which style is the default for two branch legs bound for one main lane: plain (today), fully prioritized for one leg, half prioritized (equal), or multi-to-multi? Which leg wins if prioritised (the nearer lane, or the heavier mix)? D159 says throughput decides, but no model exists yet.
2. Is a style a per-merge parameter (H24 note: "design") alongside pairing (D221), or derived from the traffic mix? One window option or per-merge?
3. Do main-to-main turning movements (LL to LL) use the same style as branch to main, given ABR06 "Merger Logics" (H26: different merges in one hub get different policy)?
4. Where may the prio of a band merge go relative to the D108 margin and to the doubled bridge (D229 order)? Not ruled.
5. Exit-side merges (D187/D205): do they need styles too, or stay plain? D210 left merge level open.
6. Is "sensitive gaps" (H25) a signal-token rule (D211), and how does it differ from SD spacing? Not specified anywhere.

Not found: no DECISIONS entry names the term "merger" as a style option; STATUS.md line 2 plans a decision card on merger styles after the S-steps.
