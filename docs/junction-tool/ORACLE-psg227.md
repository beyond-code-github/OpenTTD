# ORACLE: the rule checker on the real PSG 227 BBH (brief ORACLE-psg227, 2026-11-14)

Probe, nothing merged: `docs/junction-tool/wip/oracle-psg227-probe.patch` (test `[.oracle]` in `src/tests/junction/junction_oracle_probe.cpp`, run with `JUNCTION_ORACLE_JSON=<bbh.json> openttd_test "[.oracle]"`). Raw per-family output: `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/oracle227/<Family>.txt`. "box x,y" below is the tile offset in the trace box (335,311-392,368, in the export's own orientation); "map" is the map tile.

## Import
- Box 335,311-392,368 (58 x 58), 682 rail tiles, 0 stations; 328 signals (block 261, exit 26, combo 21, entry 18, path_oneway 2; no path signals), 11 pieces with signals facing both ways (two-way), 19 bridges, 1 tunnel (371,338 to 375,338). Nothing skipped; every signal type was expressible. A two-way piece is two `JunctionSignal` entries with opposite trackdirs (the layout cannot mark "two-way" as such).
- The checker's port side: the box is turned half a turn (`TransformJunctionLayout` Rotate twice) so the N arm (NW edge) lies on the S side; the E arm is the W side, the SW arm the E side. Topology `MakeJunctionHubTopology(LL_RR, LL_RR)`: the E-SW line is the "main line" (ML), the N arm the "branch". That assignment is arbitrary for this symmetric hub, and every ML-worded rule below (ML bridge, ML merge, prio before an ML merge) is judged against the E-SW line only.
- `FindJunctionPorts` could not be used: it needs exactly one straight axis-track tile per slot on each edge, and lane N350 leaves on a curve (350,311 Left, 351,311 Right). The 12 ports were built by hand from the trace lanes (tile and trackdir of the lane at the box edge), slots in the topology's order.
- Expected movements: the trace's 18-pair reach table, as `ctx.lanes` (`ExpandJunctionLanes` of the hub gives 24 any-to-any pairs, and lane-keeps-lane does not fit E-SW either). The probe overwrote `ctx.lanes` and each path's `movement` after `MakeJunctionTopologyCheckContext`; no `src/junction` code changed.
- Sanity (CheckConnectivity): all 18 expected movements are found, none missing, none unexpected, no "ambiguous movement" (the doubled lanes pass the doubled-lane test). It still reports 6 trace problems (below); they are the tracer walking pieces without reading signal directions, not an import fault, so the import stands. 26 paths are traced (18 movements, 8 extra doubled routes).
- Import artefact: rotated, the box rows 366-368 have no track, so Footprint also says "minimum offsets must be 0".

## Families (violations)
| Family | Violations |
|---|---|
| Connectivity | 6 (trace problems) |
| Geometry | 0 |
| Order | 8 |
| CurveSpeed | 6 |
| Bridges | 3 |
| Signals | 64 |
| WaitingRoom | 12 |
| Deadlock | 3 |
| Prio | 37 |
| Footprint | 9 (8 boundary, 1 import) |
| PrioReach | 0 |
No family asserted or crashed.

## Per family, per rule message
Readings are INFERRED from the tile data and `ANATOMY-psg227-bbh.md`.

**Connectivity (6)**
- cycle x3 (362,330; 374,339; 376,342): the cycles round the two-way pieces; the tracer follows both directions of a two-way or diagonal-cross piece.
- unexpected exit x2 (392,345 = E345 in-port tile; 355,311 = N355): a signal-blind path leaves through an in lane's tile, backwards.
- dead end x1 (370,336): an unused Y piece on an X+Y tile whose north end meets a tile with only X; a stub.

**Geometry (0)**: no tile of a used path holds X and Y, although 370,336 holds both in the export (the Y is a stub no traced path uses).

**Order (8)**
- split after a merge x7 (361,339; 373,346; 373,345; 370,349; 361,330; 368,346; 367,346): the diagonal crosses and 2x2 doublings are one block that merges two tracks and splits again ("direction cross", 4 in / 2 out). The real hub merges and splits on the same 2x2.
- adjacent splits x1 (373,346): the E345 doubling feeding the peel splits; two splits one piece apart.

**CurveSpeed (6)**: limit 132 below 160 at 358,323; 363,339; 371,349 (and 357,321; 356,322; 370,339). The N arm's diagonal approach and three core bends. At the spec's TL 4 and 160 km/h the real hub slows trains (the engineer's suspicion of TL violations holds); the spec chooses the numbers.

**Bridges (3)**
- ML bridge is not doubled x2 (372,339 and 375,338): a single bridge 372,334-372,339 and the tunnel end 375,338 sit on the E-SW line without a twin.
- tile shared with another bridge x1 (372,338): the tunnel 371-375,338 passes under tile 372,338 inside bridge 372,334-372,339's span; the real hub stacks a bridge over a tunnel. The checker counts the tile twice although one is a tunnel (INFERRED: tunnel/bridge overlap is not exempt; not checked in the code).

**Signals (64)**
- signal type does not match the setting x41 (357,329; 359,329; 361,329): the test spec's setting is block; the real hub has 67 pre-signal and path signals. Spec-driven, not a layout rule.
- signal faces against the traffic x11 (361,329; 362,332; 363,332): exactly the 11 two-way pieces; their second direction faces against the traced traffic. Real hubs use two-way signals for prio over a doubled bridge.
- signals too far apart x8 (375,345; 360,331; 363,333): gaps beyond the drag spacing, mostly across bridge spans, doubled stretches and the E345 peel run.
- split in a bridge's block: no signal between the split and the bridge x2 (361,338; 363,338) and no signal before a bridge x2 (360,338; 362,338): at the south end of the x 360-363 stack the line splits straight into the bridge head with no signal tile (the engineer's own D113 case).

**WaitingRoom (12)**
- no signal at bay boundary x11 (360,339; 362,339; 367,351): the real hub's queue bays have no signal at each whole-train boundary.
- less than one whole-train bay before the upstream split x1 (360,329): the N353 join sits less than 2 TL below the split that feeds it.

**Deadlock (3)**: a signal block holds both a split and a merge at 377,343 (E343 join stage), 373,346 and 368,346 (the 2x2 crosses). The real hub accepts that.

**Prio (37)**
- the joiner's signal before an ML merge must be Entry x7 (374,345; 374,346; 371,358): the fixed lane's joiner is a block or two-way combo signal, not entry.
- missing prio x6 (373,345; 373,346; 370,358; 367,356; 377,338; 375,343): the 3-input and 2+2 joins are not a one-row prio component beside the ML.
- ML merge is not on an axis piece x2 (377,343; 378,339): the joins sit on diagonal pieces.
- prio tile holds tracks other than the axis and a join x1 (377,342); prio tile is not in the single row beside the ML x2 (377,341; 376,341); prio track touches a used track x2 (377,342; 376,341); the first prio join must be on the merge tile x1 (377,343); prio block needs exactly one join x2; prio signal does not face upstream, farthest must be Exit and the others Combo, prio signal is not between two segments (x1 each, 377,341): the E343 join (375-378,341-343) as a whole fails the prio-element shape.
- ML block within the train length needs exactly one join x3 (373,343; 367,337; 365,337); split inside prio coverage x2 (369,337; 375,343): joins and splits inside the coverage of another join; the real hub stacks them.
- stray track x6 (375,341; 372,357; 371,337): tracks beside a prio row that belong to other lanes.

**Footprint (9)**: signal on the boundary x8 (351,311; 353,311; 355,311; 392,342-344; 335,361-362): the box cut falls on signal tiles of the arms; a cut artefact, not hub evidence. Minimum offsets: import artefact above.

**PrioReach (0)**: nothing to settle because CheckPrio found most merges without a prio component; the check ran clean, not proven.

## Over-prescription table (INFERRED)
CLAUDE.md of `src/junction` has no per-rule entry beyond the Rule checker section (split inside prio coverage, pre-signal prio, no adjacent signals in coverage); the ids are from DECISIONS.md and AUDIT-rule-coverage.md.

| Rule | Broken by the real hub | Fact / decision | Reading |
|---|---|---|---|
| Connectivity: trace without dead ends/cycles/other exits | yes (checker blind to signal direction) | D189; AUDIT "opposite directions" | Checker limit: two-way pieces and crosses are walked both ways; not a hub defect. |
| Connectivity: movements, doubled lanes | no | D189, D80, D174, D188 | Holds. |
| Geometry: no X+Y level crossing on a used tile | no | D172 | Holds. |
| Order: split after a merge | yes (7) | D189 ("not invariants"), D110 | Over-prescription: D189 already says outgoing-before-incoming is a preference; the real hub merges and splits on one 2x2. |
| Order: adjacent splits | yes (1) | D110 | Real hub accepts a split one piece after another (doubling). Over-prescription. |
| CurveSpeed: 2 TL window limit | yes (6) | D96-ff, D145; CLAUDE.md curve speed | Real defect at TL 4 / 160 km/h per the spec; whether a real defect for the hub's own design speed is open; the engineer expected it. |
| Bridges: ML bridge doubled | yes (2) | D80, D174, D188 | Single bridge and tunnel on the E-SW line. Over-prescription for a lane with a fixed role, or the hub's own saving (D174 already waives it for the SL). |
| Bridges: no tile shared by two bridges | yes (1) | D172 | Bridge over tunnel; the checker may not exempt tunnels (INFERRED). Rule gap. |
| Signals: type matches the setting | yes (41) | D20, D68, D33 | Spec-driven; the hub uses pre-signals and path signals by design. Not a layout over-prescription; the setting is a spec value. |
| Signals: facing | yes (11) | D97 | Two-way signals are a real prio style; checker does not model them. Over-prescription. |
| Signals: spacing | yes (8) | D97 | Real hub spaces up to 7-9 tiles over bridges and doubled stretches. Over-prescription. |
| Signals: split in a bridge's block / signal before a bridge | yes (4) | D113, D117 | The engineer's own rule, broken by the real hub at the x 360-363 stack. A real defect or a case D192's signal-after-split covers. |
| WaitingRoom: bay boundary signals, whole-train bay | yes (12) | D71, D83, D88 | Real hub has no signal per bay boundary; rule is a proxy for queue blocking. Over-prescription. |
| Deadlock: block holding a split and a merge | yes (3) | D117, D176, D192 | Accepted by the real hub at the 2x2 crosses and the E343 join. Over-prescription. |
| Prio: Entry before an ML merge, one-row prio component, one join per block, no split in coverage, no stray track | yes (37) | D75, D87, D107, D108, D110, D111, D116, D119 | Real hubs build three prio styles (3-input join, 2+2 cascade, path/two-way); the checker knows one. Over-prescription (D119: "my rules are just proxies"). |
| PrioReach | no (0, but little to check) | D107, D108, D116 | Not a result. |
| Footprint: nothing on the boundary | yes (8) | D58, D178 | Cut artefact of the box. No evidence. |

Caveats: the main line/branch assignment of this hub is arbitrary; rules are judged for the E-SW line as ML. The spec is `MakeTestJunctionSpec()` (TL 4, 160 km/h, block signals).
