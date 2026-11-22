# CARD: E5 crowding at bundle ends (T3, LL2)

Status: for the engineer. Written 2026-11-22 after the side probes (ng1 7372d911eb..d295346141), sibling lanes (098679a270) and the failed common-run extension (patch wip/probes/e5land/sibext-failed-098679a270.patch).

## Context
- The keep-apart conflicts left on T3 (slack 13) and LL2 (10-12) are between lanes of one bundle where the bundle opens or closes. T3: D3's last pieces (13-17 of 18) against D2 as D3 heads for its join with E3. LL2: B1#2-4 against B2 near the AB fray.
- Sibling lanes as offset curves between their first and last channel crossing (D388 spine) are built behind `sibling_lanes`. They cover mid-trunk only and change nothing (T3 13, LL2 12).
- Extending the offset range outward while the pieces match makes T3 infeasible. Near the joins, E3's prio row and E3's copy lie between D1 and D2, so "one lane apart" is false there. The real offset is one lane per track between them, and it changes at the nodes where those beside tracks open and close.
- DESIGN-sss-v2 §2 already names this: a corridor is "a face inside a bundle (between two lanes, a lane and its row, a separator and its copy, or inside a crossing lattice). Rigid by rule: its width is exact." Today the drawing program makes rigid only a beside track against its parent chain (corridor family, D421) and sibling lanes between crossings. Every other inner-bundle face gets pairwise keep-apart with sides read from drawings (D418), and that is where the slack sits.

## Options
1. **Corridor faces rigid (D388 as designed), built generally.**
   - Per stretch, the tracks of a bundle (lanes, rows, copies) have a lateral order read from the track map's faces; each adjacent pair is offset by exactly one lane (the corridor row form).
   - A track that enters or leaves the order at a node does so by an exact one-lane opening or closing step, as the corridor family's beside tracks do now.
   - Pairwise keep-apart (D418) remains only between bundles (free faces).
   - Consequence: the conflicts left are inside corridors, so this removes them by construction or names the node rule that cannot hold (an upstream gap).
   - Cost: a real design pass, which I would do. It needs (a) which faces are corridor faces, read from the map, (b) the lateral order and its changes at nodes, (c) how the node rules (D398-D401) meet the opening and closing steps. Then one probe on the five hubs behind an option.
2. **Node-zone rule only.** Leave mid-trunk as now and add a rule for the stretch between a bundle's last crossing and its node (lanes peel off in lane order, each at least one lane from the next).
   - Consequence: smaller, but it is a special rule for one zone, where option 1 is the general one.
3. **Park the T3/LL2 crowding.**
   - T1, T2 and 197 are legal. Move to the roadmap items (corridor family on by default in `CheckPlanarDrawable`, coverage, node legs), and come back when Slots is built on the same rows.
   - Consequence: E5's verdict on T3 and LL2 stays "not legal" for now.

## Recommendation
Option 1. It is the plan of record (D388, DESIGN-sss-v2 §2 and §3.3) rather than a new rule, every probe so far points into corridor faces, and Slots needs the same rows later. Time-box: one design note plus one probe. If the probe still leaves slack on T3 or LL2 inside a corridor face, take option 3 and record the gap.

## Prior art checked
D381, D388, D398-D401 (nodes are rules), D403 (spine legs exemption), D417/D418 (sides from drawings), D421 (corridor family), SPEC-slots-lengths §11, DESIGN-sss-v2 §2/§3; the side-probe IIS (STATUS); Gemini 2026-11-22 sibling lanes (card CARD-E5-flexstep last bullet).

Reversible: yes (behind an option until measured).

## Gemini's reply (2026-11-22, prompt wip/gemini/2026-11-22-bundle-ends.md), digested
- Agrees: corridor faces. Recommends option 1, not 3: T3 and LL2 are the only multi-lane hubs with prio rows and copies (197 has neither), so parking them leaves staggered joins unverified before Slots. Not 2: LL2's conflict is at a fray, not only at a join tail.
- **Amendment worth taking:** exact (= one lane) only for beside tracks against their parent and for sibling trunk stretches (both built). In node-zone corridor faces (staggered joins, frays) use **at least one lane, with the side fixed from the planar map's face order**, not from a drawing: joins may need to widen for bays and curve room. This keeps the topological side, which removes the wrong-side trap, without over-constraining.
- Corridor face test (its proposal): two longitudinal side chains of parallel-ish pieces, closed by end caps that are nodes or transverse crossings. Traps: exclude crossing-lattice faces (they already have their own crossing and deck rules); a channel crossing through the bundle cuts a corridor into two faces, whose caps are the crosser's segments.
- Step collisions: curve rows alone do not order steps on different tracks; it suggests a minimum travel between consecutive opening and closing steps along the face boundary.
- **My check:** its "same bundle" membership test fails at exactly the faces that matter (D lanes against E3's prio row and copy belong to different channels). The test has to come from geometry and the map: a wall pair is "beside" when the two walls run alongside each other in the face, not end to end. The map gives the side for every wall pair in a face, but the side means "along a's normal" only for beside pairs; for walls that meet end to end the separation is along the travel. That beside/end-to-end classification is the design work for option 1.

## Outcome (2026-11-22, ng1 625835dbd3, merged to feature 948356b4e1; logs wip/probes/e5land/facing-*.log)
- `facing_sides` (D426 probe): a pair whose face walk is monotone both ways takes its side from Shape's headings. Only anti-parallel facing pairs occurred (T2 8, T3 18, LL2 17, 197 6); none of them stretched. T3 13 and LL2 10 unchanged (with sibling lanes too: T3 13, LL2 12). T1, T2 and 197 stay legal.
- Every keep-apart row still stretched is a pair the walk rule cannot decide: corner pairs at bends. T3: D3 against D2 at 45 degrees where D3 bends towards its join (75 2 15, 75 5 8) and B2/B1 at 45 degrees (29 1 10, 29 4 9). LL2: eastbound D1 turning NE to E against westbound B1 turning SW to W, half a tile apart at about (99, 58.5), two different bundles (sep 33 rows), plus B2#4 / B1#0-1 (37 6 11, 37 6 12).
- Reading: sides guessed wrong inside corridors were not the cause. What is left is corner geometry at bends where neither normal is certain from headings, and the earlier joint probe (binaries on just these pairs: T3 12, LL2 7) says the room is short jointly even with free sides. That points at Shape's headings putting bends too close for the lengths to open (an upstream promise question: corner clearance between bends of neighbouring tracks), not at the drawing program.
- D426's tripwire applied: T3 and LL2 parked; the options stay off by default. Gap recorded: corner clearance between facing bends of neighbouring tracks (same or different bundle) is neither promised by Shape nor decidable from headings in the drawing program.

## Gemini's zoom-out reply (2026-11-22, prompt wip/gemini/2026-11-22-zoom-out.md), digested
- Its view: the main problem is Shape's uncoordinated headings in multi-lane prio joins (H14 aligns lanes only at channel crossings), made worse by our own prio element (rows and copies). Not the pipeline's heuristics: 197 solves cleanly. Literature: fixed heading sequences can make an embedding undrawable whatever the lengths; routers prevent it upstream with coordinated bundle/join turn patterns (our D310 / catalogue idea, deferred by D401 on 197's evidence).
- Tests: (A) remove the prio element, ideally with the main lanes' headings frozen; (C) exact single-face solves with a tight coordinate bound (its "< 2 s" timing is a guess). (B) the PSG 227 answer key checks base rules only, since PSG 227 does not use our prio element.
- Taken: (C) as specified; (A) by removing the prio statement and diffing the main lines' headings as the control, instead of a frozen-heading loader. Brief wip/BRIEF-E5-certificates.md.

## Certificates (2026-11-22, ng1 856ca51153..a973e7081f, merged; logs wip/probes/e5land/cert-*, face33-*)
Exact solves: every keep-apart pair in the listed faces binary, inserts free, coordinate bound 256 (big-M 1024); other faces' keep-apart left out, so Infeasible is a proof and Optimal shows the faces fit.
- **T3:** faces 75, 29, 66 singly and together Optimal (0.3-3 s). T3's slack is the pipeline's heuristics (sides and inserts chosen round by round), not geometry.
- **LL2:** face 33 Infeasible; 34 and 37 Optimal. Without the prio statement LL2's headings are identical (52/52) and its slack is unchanged, so prio rows are not the cause.
- **LL2 face 33's minimal core (deletion filters):** keep-apart `sep 33 3 7` (D1#11 E | B1#1 SW) and `sep 33 3 10` (D1#11 E | B1#4 W), plus one row, `corridor F1"0 close`: F1's copy (one piece heading S, one lane on F1's left) rejoins F1 at node 25 by the closing vector (-2, 2). Curves and bays play no part. The closing vector is the one diagonal step that brings a one-lane-offset track back onto its parent, so it is forced geometry, not a convention. So the face is undrawable because F1's copy rejoins F1 inside the face where D1's NE-to-E bend and B1's SW-to-W bend face each other; D1#11 cannot clear both B1 pieces then.
- Reading: LL2's blocker is upstream. Structure ends F1's copy (node 25), and Shape puts two facing bends in the same face. Neither layer promises room for that. T3's blocker is in the drawing pipeline.
- Supersedes the "corner clearance" suspicion above and PAPER C10's negative part.
