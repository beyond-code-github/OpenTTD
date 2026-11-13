# Junction signals (src/junction/signals/)

- `junction_signal_pass`: only `PlaceDragSignals`, called by `junction_signals.cpp`, is current; `PlaceJunctionSignals` is superseded and not extended.

## Signals and prio
- Default signal type is one-way block, a junction-only client setting separate from the signal toolbar's type.
- Signal spacing SD is the game's signal spacing when dragging (`drag_signals_density`), applied as `CmdSignalTrackHelper` does: logical length 192 per axis piece and 128 per diagonal half piece, density SD x 192, minimise-gaps mode.
- Fill walks upstream from the downstream fixed signal, so the short block falls at the upstream end and bays line up on the grid.
- Signals are graph tokens placed by rule; a run floor that exists to hold a signal becomes a Signal token with its own rule.
- A legal signal piece is not on a bridge, a head or a split tile and has no overlapping track on its tile.
- BeforeSplit and BeforeMerge: the last legal piece before the split or merge node; AfterSplit and AfterMerge: the first legal piece after it; BeforeBridge and AfterBridge likewise; Stub: the piece before the boundary tile.
- Every bridge and tunnel is its own block: a signal on the nearest legal piece before the near head and after the far head, on every path.
- Between a split and the next bridge head there is a signal-legal piece with a signal; likewise a merge after a far head.
- A signal straight after every line-choice split stands; no minimum length from a split to the next bridge head.
- One signal may serve several rules of its stretch; no back-to-back signals where one suffices.
- Waiting bays: every queue ending at a merge signal has boundary signals at least 2 TL units apart going back from the merge signal, each boundary on a legal piece that is not a split of the path; bays may contain a bridge; at least one bay before the upstream split.
- A train stopped in a bay does not hold a split, so a held train never blocks the main line.
- Bays add signals and never replace SD spacing; following trains keep close headway.
- Prio: a merge onto the main line carries a prio, a parallel unused track joined at intervals upstream so a main-line train holds the merging signal red.
- Prio signals are always pre-signals (entry, combo, exit) regardless of the junction signal type; one prio segment per main-line block.
- Prio joins are trailing pairs of 45-degree pieces; an unused-track join is a prio connection, not a merge.
- The prio's first join may sit on the merge tile, where it holds only its half piece with no axis stub.
- A prio's length comes from the picked train: the gap between the main-line train's front and the joiner's tail never drops below one signal block while the joiner accelerates to design speed.
- Prio coverage is in whole tiles, sized by exact timing; coverage above 32 tiles is refused ("Prio coverage would exceed 32 tiles"); the prio element's joins sit at even offsets, so an odd length is built one tile longer.
- Every main-line lane that has a joiner (a merge whose other edge is not a row edge) has a prio element; the throughput estimate does not decide it on the graph pipeline.
- Main-line delay from the junction is strictly zero by default.
