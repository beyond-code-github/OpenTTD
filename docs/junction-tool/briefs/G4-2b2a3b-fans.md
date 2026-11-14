# G4 2b2a3b: O2, adjacent W/E port fans separated at their end rows (DESIGN-G4 §3 O2; D271)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
The fan of a W/E port is the S-bend of a row's In or Out edge between its port row and its stack item. It can stand on another row's rows. Today that fan is separated from its neighbours only by the flat sort, at a point.

The static model (DESIGN-G4 O2) works pair by pair. For each pair of adjacent fans of one side, belonging to different rows, it evaluates both fans at the rows of their four ends: two port rows and two item rows. At each end row, it gives each fan's tiles as candidate pieces with row conditions, and orders the pair by a fixed rule read from the port rows and each fan's direction.

Two facts from RECON-A6 §4-5 make this enough at the seeds. A foreign fan on an item row lies outside every ranked occupant of that row except its adjacent fan. Every row shared by two adjacent fans is one of their end rows.

This step records those rows as data. Nothing reads them yet: no layout, objective, hash or digest moves.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`: run `git -C /home/pete/OpenTTD/.claude/ng1 checkout -B wt-2b2a3b <HEAD given at launch>`. Leave its untracked files alone.
- `build-rel` (Release) exists. Delete stale `.pch` files if the build complains.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/2b2a3b/`.
- A GUI game may be running from `ng1/build-rel/openttd`. Build only `openttd_test`, and never touch that process.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md` (C++ part).
- `docs/junction-tool/DECISIONS.md`, row D271.
- `docs/junction-tool/RECON-A6-crossers-fans.md` §2 ("Horizontal start" forms) and §4-6 (lines 40-104). The table of the 8 fans and the four pairs are your expected values.
- `docs/junction-tool/briefs/G4-2b2a3a-crosser-pieces.md`: the previous step. This step reuses its mechanism (`JunctionBendRowForms`, guarded `JunctionStaticRow`s, condition rows).
- `src/junction/placement/junction_placement.h` lines 130-245.
- `src/junction/placement/junction_placement.cpp`:
  - 215-240 (`Elem`: `slot`, `port_first`, `index`, `m`, `hand`);
  - 720-735 (a fan's hand from its rows);
  - 830-845 (where a Fan element is made);
  - 1228-1262 (`row_pieces`: the port Tile, the Fan, and the run between them);
  - the 2b2a3a candidate block (grep `The crossers' candidate pieces`).
- `src/junction/topology/junction_topology.h` lines 40-66 (`JunctionPortSlot`, `JunctionPortSlots`).
- `src/tests/junction/topology/junction_topology.cpp`: the item-row and crosser tests (grep `crossers: one piece covers`).

## Definitions
- **Fan.** A Fan element of a row's In or Out edge chain (`chain[e]`, copy 0) whose port slot is on the W or E side, with `slot >= 0`. Its port row is `P = port_offset + n`, with `n = port_row[slot] - port_offset` as data and the offset O a term. Its item row is `y[item_pt[el.index]]`. Its first tile's row is P when `port_first`, else the item row. Its first tile's column is `x[el.first]`. Its forms are `JunctionBendRowForms(el.hin, el.hand, false)`, and its length is the system bend's L (`bend_ids`).
- **Its port tile and run.** These are the chain's Tile element with the same slot, and the run between that Tile and the Fan. Both stand on row P, as `row_pieces` writes them:
  - the port tile has columns `{tile.first, 0}` on both ends;
  - the run goes from `{west point, +1}` to `{east point, -1}` between the Tile's point and the Fan's nearer point (the fan's `first` when `port_first`, else its `last`);
  - a run that `row_pieces` skips (a Fan of `m == 0` next to the port Tile) is not a candidate.
- **Side.** W when the slot's side is W in `JunctionPortSlots` order (the W slots come first, north to south), E when it is E.
- **South.** A fan is south when its item row lies south of its port row at the recording (`vy[item_pt] > port_row[slot]`), else north. This is the σ of DESIGN-G4: data now, a binary in 2b3.
- **Adjacent pair.** On one side, take the fans sorted by `n`. Two consecutive fans whose edges belong to different rows are adjacent.
- **Order rule.** It decides which fan of the pair is west, f, and which is east, g. Use it only when both fans are south or both are north:
  - W side, both south: the fan with the larger n is west.
  - W side, both north: the fan with the larger n is east.
  - E side, both south: the fan with the larger n is east.
  - E side, both north: the fan with the larger n is west.

  At the seeds this gives e0 before e1, e20 before e19, e6 before e5, and e24 before e25 (that pair shares one tile). When the two fans' directions differ, record no rows for the pair and count it in `fan_pair_crossing`.
- **End rows.** There are four:
  - 0: f's port row;
  - 1: f's item row;
  - 2: g's port row;
  - 3: g's item row.

  Each is a row expression: `y[item_pt]` for an item row, `O + n` for a port row.

## Data (`junction_placement.h`)
- **`JunctionStaticRow`** gains `int o = 0; ///< The coefficient of the port offset O on the left side.` and `std::vector<int> fan_pieces; ///< The fan pieces it holds under (indexes of fan_pieces), all chosen.` Its comment adds `+ o O`.
- **New, after `JunctionCrosserPiece`:**
  ```cpp
  /** Two adjacent W/E port fans of one side (DESIGN-G4 O2), west then east by the order rule. */
  struct JunctionFanPair {
  	uint32_t west, east; ///< The fans' edges.
  	bool east_side; ///< On the E side, else W.
  	bool south; ///< Both fans run south from their port rows (σ at the recording).
  };

  /** A piece of a fan of a pair that may stand on one of the pair's end rows (DESIGN-G4 O2): its port tile, its run to the fan, or one of its S-bend's row segments. */
  struct JunctionFanPiece {
  	int pair; ///< Its pair (index of fan_pairs).
  	int end; ///< The end row: 0 the west fan's port row, 1 its item row, 2 the east fan's port row, 3 its item row.
  	uint32_t edge; ///< The fan's edge.
  	std::string kind; ///< "port", "run" or "bend".
  	int bend; ///< The bend's id for "bend", else -1.
  	int segment; ///< The segment of JunctionBendRowForms for "bend", else -1.
  	bool chosen; ///< It stands on the end row at the recording.
  };
  ```
- **`JunctionPlacementSystem`** gains, after `crosser_uncovered`:
  ```cpp
  std::vector<JunctionFanPair> fan_pairs; ///< The adjacent W/E fan pairs whose rows are ordered.
  std::vector<JunctionFanPiece> fan_pieces; ///< Their pieces at their end rows, by pair, end and fan.
  int fan_pair_crossing = 0; ///< Adjacent pairs whose fans run in different directions: no rows.
  ```

## Changes (`junction_placement.cpp`, in the record block after the crosser candidates)
1. **Lifecycle.** Add `recorded_fan_pairs`, `recorded_fan_pieces` and `recorded_fan_pair_crossing`. Clear and copy them exactly as `recorded_crosser_pieces` is cleared and copied.
2. **Pairs.** Collect the fans and the adjacent pairs, and apply the order rule.
3. **Pieces at each end row E of each pair, for each of the two fans.** Let the fan's first tile row be `Yf` (P or the item row). Every condition row has reason `"O2 pair <p> end <e> e<edge> piece <n>: <port | run | bend <id> seg <s>> <lo | hi>"` and `fan_pieces = {n}`.
   - **Port tile and run** (when present). Two condition rows, `E - P >= 0` and `P - E >= 0`. Its tiles:
     - port: one tile `{tile.first, 0}`, bits X;
     - run: columns as above, bits X; an empty run (right before left at the recording) is still a candidate and pairs with nothing, as the empty-run rule of the item rows.
   - **Each segment s of the fan's forms.** With `d = forms.d`, two condition rows: `d (E - Yf) - lo1 L >= lo0` and `hi1 L - d (E - Yf) >= -hi0`. Its tiles: per form tile t, columns `{el.first, t.c0}` both, plus `t.slope * d * (E - Yf)`, bits `t.bits`.
   - **Writing them as rows.** A row expression `y[q]` adds `(q, coefficient)` to `y`. `O + n` adds the coefficient to `o` and `coefficient * n` to the constant: move it to the gap with the opposite sign. A tile's `(E - Yf)` term likewise goes into `y`, `o` and the constant.
   - **chosen.** The piece's two condition rows hold at the recording's rows, offset and lengths. Several pieces of one fan may be chosen at one end row: the port tile, the run and the fan's first segment all stand on its port row.
4. **Rows.** At each end row, for every tile a of a piece of f and every tile b of a piece of g:
   - the row is `b.west - a.east + (b's y, o terms) - (a's y, o terms) >= gap`;
   - the gap is 0 when both tiles are "bend" tiles and `JunctionUnrelatedTileGap(a.bits, b.bits).share`, else `.apart`;
   - `fan_pieces` is both pieces;
   - the reason is `"O2 pair <p> end <e>: e<f> <kind> / e<g> <kind>"`.
5. **CLAUDE.md** (placement, standalone facts). Add after the item-row fact:
   > Two adjacent W/E port fans of one side are ordered at the rows of their four ends (their port rows and their item rows): on the W side the fan with the later port slot is west when both run south and east when both run north, mirrored on the E side; each fan's port tile, run and S-bend segments stand on an end row under their row conditions, and two fan S-bend tiles may share a tile.

## Tests (red first: add the members and leave them empty, build, see each new test fail on an assertion, record the messages)
In `junction_topology.cpp`, after the crosser tests, using `RecordItemRowSeed`. Evaluate O as `system.port_offset`.
1. **"O2: every fan row holds at the seeds" `[junction][topology]`** (T, LL_RR x L_R; L_R x LL_RR as a separate `[topologysweep][.sweep]` case).
   - LL_RR x L_R has 4 or more pairs, and its pairs include (e0, e1), (e20, e19), (e6, e5) and (e24, e25) in that west-east order.
   - `fan_pair_crossing == 0`.
   - Every static row whose `pieces` and `fan_pieces` are all chosen holds at `solved`.
   - WARN per hub: pairs, pieces, chosen, rows, and gap-0 rows.
2. **"O2: a chosen fan piece's tiles are the fan's tiles"** (same hubs).
   - Evaluate each chosen piece's tile columns at `solved`, at the end row's value.
   - The oracle comes from `system.chains` and `system.bends`:
     - the fan's tiles are `x[first]` plus `JunctionBendTileForms(hin, hand, m, false)` offsets, from the first tile's row (P or the item row by `port_first`);
     - the port tile and run come from the chain's port Tile point.
   - Compare as column sets on that row.
3. **"O2: chosen fan pieces cover every shared row of the seeds' pairs"** (LL_RR x L_R).
   - For each pair, compute at `solved` the rows where both fans have a tile, from the oracle of test 2.
   - Each such row equals some end row's value, and at that end row both fans have a chosen piece.
   - RECON-A6 §5 found 0 interior shared rows.
4. **Update "item rows: gap 0 only between band-edge pieces":** O2 rows with gap 0 are allowed when both reason kinds are "bend".

## Do not
- Feed any row to a solve. Change no existing relation, clash or digest, and not `add_occupancy`'s flat path. Leave the item rows' skipped foreign-fan occupants as they are.
- Add a binary, σ as a variable, S fans (SFan), O3, or clearance.
- Touch any worktree but `ng1`, or the running game. `.claude/nodes2` is reserved for the engineer's game.
- Use `pkill`, a broad `kill`, or any form of `git stash`.

## Stop conditions
- A fan has no port Tile with its slot.
- A seed pair's order differs from the rule.
- A seed's fans run in different directions in a pair.
- A chosen piece's tiles differ from the oracle.
- A shared row is not an end row.
- A row fails at a seed.
- Any identity value moves, or any existing test fails.
- The gate or sweep goes over budget because of this step. Compare against a run at the launch head on the same machine state, and report both times. The sweep was at 54.9 s of 60 at the launch head.
- A condition you cannot write in the row form above.

## Gates (D264)
- `tools/junction/jt gate build-rel`, `tools/junction/jt sweep build-rel` and `tools/junction/jt identity build-rel`, each with a Bash timeout of at least 400000 ms: all pass; report cases and times.

One commit on wt-2b2a3b: `Add: Junction recording orders adjacent port fans at their end rows`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- The references read; the commit; the red evidence per test.
- The types and members added.
- Per hub: pairs (by side, with south or north), pieces, chosen, rows, and gap-0 rows.
- Gate, sweep and identity: cases and time. Stops. `git status --porcelain` and `git log -1`.

When you have reported, stop.
