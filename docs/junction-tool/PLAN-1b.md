# Milestone 1b: T-junction generator

**Status:** full plan, 2026-11-07. It's waiting for the engineer's review before any step runs.

1b replaces 1a's fixed test layout with a generated one: an L_R-style full T-junction (D17, D22). It's derived from the rules and parameters (D26). The output is a `JunctionLayout` (1a), so placement, the ghost, pricing and building work unchanged.

**Rests on:** D16–D27, D29–D31, D33, D35–D38, D45, D58–D74. **Rules:** `RULES-coop.md`. **Code facts:** `RECON-generator.md`, `RECON-path-prio.md`, `RECON-placement.md`.

**Hard constraints for every step:**
- rebase-friendly: new code in `junction_*` files, and upstream edits are seams only (D42)
- house UI style (D48)
- no savegame format change of any kind (D69)

## What the player gets

**Junction window:**
- **Mainline notation and branch notation** edit boxes (D38, D60). Gaps are accepted (`L_R`, `L__R`, ...). A multi-lane notation shows "multi-lane lines aren't supported yet" (D72).
- **TL** with +/- buttons, standing in for 1c's train picker (D64).
- **Rotate, Mirror and cost line,** as in 1a.
- **A status line** saying why generation failed, e.g. "no layout found within 64×64".
- The 1a test layout is gone from the UI. It stays as a test fixture.
- The notations and TL are remembered in `openttd.cfg` (D69).

**Settings window:** a new client setting "Junction tool signal type" (one-way block or one-way path, default one-way block; D68). It sits next to the existing signal settings.

**Taken from existing settings:**
- **SD** comes from "signal spacing when dragging" (`gui.drag_signals_density`, D65).
- **Driving side** comes from the road-side setting (`vehicle.road_side`). The canonical layout is right-hand, and a left-hand game gets it mirrored (D66).
- **CL is derived and never entered** (D67). It's computed from TL, the reference top speed (the fastest buildable compatible engine, capped by the rail type's limit; D45) and the rail type's curve bonus.

**Behaviour:**
- Changing any input regenerates the layout once, and the ghost and price update.
- Hover pricing is cached per origin, orientation and inputs (D63).
- Building uses 1a's sequencer, at about 500 steps and roughly 15 s. Track runs aren't batched in 1b (RECON-generator §4).

## Design

### 1. Frame and conventions

- **Canonical frame,** in tile coordinates:
  - The mainline (ML) runs along x. Track **E** carries +x traffic (`X_SW`) at row `yE`, and track **W** carries −x traffic (`X_NE`) at row `yW = yE + gML + 1`, where `gML` is the gap count from the ML notation.
  - The branch lies on the +y side, along y. **Inbound** (`Y_NW`, towards the ML) is at column `xI`, and **outbound** (`Y_SE`) at `xO = xI + gBR + 1`.
- **Right-hand rule as derived:** on screen +x points SW and +y points SE. Traffic heading SW has NW (−y) on its right, so right-hand +x traffic takes the lower-y track.
  - The first manual check confirms this on screen.
  - If it's wrong, one constant flips (`kCanonicalRightHand`).
  - When road side is Left, the finished layout goes through `TransformJunctionLayout(…, Mirror)`.
- **Units:**
  - Lengths along track are counted in **half-tile units**: an axis piece (X/Y) is 2, a half piece (Upper/Lower/Left/Right) is 1, a bridge tile is 2. This matches the game's wagon positions (D36).
  - TL in units is `2 × TL`. SD in units is `2 × SD`.
  - A *half piece* is what the coop guides call a diagonal piece; avoid OpenTTD's `IsDiagonalTrack` wording (RECON-generator, vocabulary trap).
- **Directions:** a table `TrackdirToDirection` maps every trackdir to its compass `Direction`:
  - `X_NE`→NE, `X_SW`→SW, `Y_NW`→NW, `Y_SE`→SE
  - `Upper_E`/`Lower_E`→E, `Upper_W`/`Lower_W`→W
  - `Left_N`/`Right_N`→N, `Left_S`/`Right_S`→S

  A 45° turn is left or right by `DirDifference` (`Left45` or `Right45`), which is the same test the game's curve code uses.

### 2. Topology of the full T (fixed; derived, not searched)

| Movement | Turn | Crossings |
|---|---|---|
| E→E, W→W | straight | none |
| W→S | right | none |
| N→W | right | none |
| E→S | left | crosses track W (under the ML bridge) and N→E (the crossover) |
| N→E | left | crosses track W (under the ML bridge) and E→S (the crossover) |

- **Switches:**
  - E has a split P (E→S) before its merge Q (N→E).
  - W has a split D (W→S) before its merge A (N→W).
  - The inbound branch has a split S_I.
  - The outbound branch has a merge M_O (W→S with E→S).

  That's 3 splits and 3 merges (R10). Splits before merges holds on every track (D24).
- **Planarity argument** (it fixes the order of features along W):
  - Below W, the chords to the inbound branch must attach to the left of the chords to the outbound branch.
  - Between E and W, E's split comes before its merge (`x_P < x_Q`).
  - So, on W, the order from low to high x is **A (N→W merge) < B (E→S crossing) < C (N→E crossing) < D (W→S split)**, and E→S and N→E cross exactly once, below W.
  - The other order (C < B) puts the crossover in the gap between E and W. That's design choice X2, only feasible when `gML` can hold a bridge.
- **Grade separation:**
  - B and C pass under an ML bridge carrying W: one bridge over both, or two (design choice X3).
  - The crossover is a bridge carrying either E→S or N→E over the other (design choice X1).
  - Diagonal track may pass under bridges (D61).
- **Prios** (D25, D27): one on each ML merge, Q on E and A on W. Each prio's side is a design choice (X4/X5, D70): either side of its ML track, as long as it touches no other track.

### 3. Route programs (how geometry is written)

- **A route is** a start `(tile, trackdir)` plus a move string. The moves:
  - `F`: straight on. Next tile from the exit diagdir via `TileIndexDiffCByDiagDir`; next trackdir via `NextTrackdir`.
  - `L` / `R`: a 45° turn. The member of `TrackdirReachesTrackdirs(cur)` whose `Direction` is `Left45` or `Right45` of the current one.
  - `B<n>`: a bridge with `n` middle tiles along the current axis trackdir. The head tiles get no track (RECON-generator §2), and the route continues after the far head.
- **The rasterizer** turns a route into pieces `(tile, trackdir, units, on_bridge)`.
- **The layout** is the union of all routes' pieces:
  - track bits ORed per tile
  - bridge heads and spans recorded as `JunctionBridge`, north head first
  - prio structures and signals added afterwards

  Splits and merges aren't written explicitly: they appear wherever routes share and then part.

### 4. The rule checker (the oracle)

`CheckJunctionRules(const JunctionLayout &, const JunctionSpec &) -> std::vector<RuleViolation{rule, tile, message}>`.

- **It works from the layout alone,** using a **route tracer**: from each entry leg's edge trackdir, follow the trackdirs actually present (`TrackdirReachesTrackdirs` ∩ the next tile's tracks, crossing bridges head to head). It enumerates every path to every exit.
- **The checker never sees the generator's route programs.** It stays an independent test oracle.
- **Each group has its own failing fixtures,** and each fixture breaks only that rule.

| Group | Rule | Source |
|---|---|---|
| G1 connectivity | Exactly the six movements, one path each, entry edge to exit edge. No other reachable exit, no cycle, no dead end on a route. | D22, R10 |
| G2 geometry | No tile where two routes use crossing tracks (level crossing). A 90° crossing is allowed only as a prio rung (one track used by no route). No 90° turns (structural). | R1, grade separation |
| G3 curve speed | Slide a train of `2×TL` parts (1 unit each) along every route, one unit at a time. At each position, apply `Train::GetCurveSpeedLimit`'s formula exactly (D67), counting direction changes between consecutive parts: the 88 cap for bends ≤ 1 part apart, the 61 cap for 90°, the S-bend exemption, `232 − (13 − s)²`, and the curve bonus. Violation if the limit is ever below the reference speed. | D18, D36, D67, R1–R7 |
| G4 order | Along every route, no merge before a split. | D24, R8 |
| G5 bridges | No track and no signal on head tiles. The tiles before and after the heads carry the axis track. Spans don't overlap other spans or heads. Every span length is accepted by `spec.bridge_span_ok(len)`. | RECON-generator §2, D35, D45 |
| G6 signals | Every signal is on a legal tile (`!TracksOverlap`, not a head). Route signals face along traffic. A signal is on the nearest legal tile before every split, before every merge on each incoming track, and after every merge. Along each route, no gap between signals is longer than SD units while a legal untaken tile exists in the gap. Signal types follow D68/D74. | D20, D21, D65, R32, R38 |
| G7 waiting room | On each connection leaving the ML (E→S, W→S), the track from the ML split's switch tile to that connection's last signal before its merge is ≥ `2×TL` units. | D71a, R9, R15 |
| G8 no deadlock | No signal block contains both a split and a merge. Together with G4, G7 and one lane per direction, this rules out wait cycles. The argument is written in the test file. | D71b |
| G9 prio | Each ML merge has a prio track parallel and adjacent to its ML track, on either side. It's linked only by rungs: an ML tile with X+Y, a prio tile with X+Y, and no other connection. Its ML blocks start at the merge block and run upstream until they cover `TL` tiles (1b's stand-in length; 1c replaces it, D29/D34). Each ML block in range has exactly one rung, to its own prio segment (R25). Segment 1's rung is downstream of the merge switch, inside the merge block, and the ML's after-merge signal is beyond it (D59, R26). The joiner's merge signal is Entry. Prio signals face upstream: Combo, except the farthest is Exit (D74). Rungs and prio tiles touch no route track. | D25, D27, D59, D70, D74, R23–R26 |
| G10 footprint | `size` is the bounding box. Every leg's tracks run flush to that side's edge. The outermost switch, signal, prio piece or bridge head on each leg is at least 1 tile in from the edge. | D58 |

A bridge block can join a prio: a rung on a plain tile of the bridge's block, just past the downstream head, links the whole bridge span (inferred from block flooding, RECON-path-prio §2). The checker treats any ML block in range that way.

### 5. The generator

`GenerateJunction(const JunctionSpec &) -> JunctionGenerateResult{std::optional<JunctionLayout>, StringID error}`. It's pure and deterministic.

1. **Inputs (`JunctionSpec`):**
   - the parsed ML and branch notations
   - TL
   - SD (from the setting)
   - the reference speed
   - the rail type's `curve_speed`
   - the signal type
   - `bridge_span_ok(len)`
   - unit prices (rail piece, signal, bridge by length), used only for tie-break scoring

   The game side fills these in `junction_game.cpp`.
2. **Derived CL:** `DeriveCurveSpacing(speed, curve_speed, tl)` returns the smallest `s` in units whose limit ≥ speed, capped at `2×TL`. Templates use it as the minimum half-piece run between same-handed turns. G3 remains the judge.
3. **Templates:** one route program per movement in the canonical frame. Each has integer variables for the positions along the source line, the diagonal run lengths (≥ derived CL), the bridge spans, and the prio placement.
   - They are written up in `TEMPLATES-1b.md`, designed by the orchestrator in step C together with a hand-checked reference layout.
   - Variables are computed from closed-form minimums (CL, TL waiting room, bridge minimum span, prio cover). Each macro's displacement is measured by rasterizing it once, never by hand arithmetic.
4. **Search:**
   - over the design choices X1–X5, for each choice a small slack sweep (0..3) on the variables that `TEMPLATES-1b.md` marks as sweepable
   - for each candidate: build, add signals and prios (§6), run `CheckJunctionRules`, and keep the rule-clean ones
   - scoring: fewest occupied tiles (heads and spans included), then the unit-price cost (D30)
   - a candidate cap keeps it deterministic, and the 64×64 bound gives "no layout found" (D62)
5. **Errors:**
   - a multi-lane notation (D72)
   - an unparsable notation
   - no rule-clean candidate. The message names the rule group that failed most often, e.g. "curves too tight for this train", "no bridge fast enough".

### 6. Signal and prio placement (a generator pass)

1. **Switch isolation:**
   - On the nearest legal tile before every split, before every merge on each incoming track, and after every merge, place a one-way signal along traffic, of the setting's type.
   - The joiner's signal before an ML merge is Entry.
2. **Fill:** along each route, place a signal on the first legal free tile once the distance since the last signal reaches SD units. Shared pieces share signals.
3. **Prios:**
   - For each ML merge, on the chosen side, lay the prio row from one tile past the merge switch upstream until the blocks cover TL.
   - Put one rung in each ML block, with prio signals between segments facing upstream: Combo, and Exit for the last.
   - Rung tiles can't take ML signals (overlap), so the rung goes on a plain tile in the block that doesn't already hold an ML signal.

### 7. Game side

- **`junction_game.cpp`:**
  - `MakeJunctionSpec(railtype, notations, tl)` reads the settings, `road_side`, the rail type, prices, and `GetJunctionReferenceSpeed(rt)`. That function is factored out of `ChooseJunctionBridgeType`'s engine search (D45), then capped by the rail type's limit.
  - `bridge_span_ok` uses `ChooseJunctionBridgeType(rt, len)` plus speed ≥ reference.
  - The original acceleration model has no curve limit, so G3 is skipped under it. Revisit in 1c.
- **Window (`junction_gui.cpp`):**
  - It holds the generated layout and regenerates in `OnEditboxChanged` and `OnClick` for TL.
  - It shows the error text, and uses the cached layout for hover and building.
  - Hover estimate cache per D63.
- **Settings:**
  - `gui.junction_signal_type` (`SDTC_VAR`, shown in the Settings window next to the signal settings)
  - `gui.junction_ml_notation` and `gui.junction_branch_notation` (string; copy the `SDTC_SSTR` template into `gui_settings.ini`, RECON-generator §5a; not shown)
  - `gui.junction_tl` (`SDTC_VAR`, not shown)
  - all with `NotInSave` and `NoNetworkSync`
- **Strings** in `english.txt` under `STR_JUNCTION_*`.

### 8. ASCII renderer

`RenderJunctionAscii(const JunctionLayout &) -> std::string`: two characters per tile, with a fixed legend:
- the track shape
- `#` for a switch tile
- `+` for an X+Y crossing
- `H` for a bridge head, `=` for a span
- `>` and similar for signals by facing
- `p` for prio track

A hidden Catch2 test (`[.render]`) writes `docs/junction-tool/layouts/*.txt` for a parameter sweep, for engineer review. Tests never compare pictures (D26).

## Steps

Grouped for token efficiency (D41, D52). Each agent costs about 40–50k tokens before it starts, so steps of the same kind share an agent. There are four Sonnet agents and one orchestrator step, each briefed in `briefs/1b-step*.md`. Every step runs the build and the full suite. The test baseline is 133 cases.

| # | Step | Contents | Outcome |
|---|---|---|---|
| A | **Foundations** (agent) | `junction_generator.h/.cpp` and `junction_rules.h/.cpp` with stubs. The outer test "GenerateJunction gives a rule-clean L_R T for TL4/SD2 at 160 km/h" is tagged `[!mayfail]` until D; the `CheckJunctionRules` stub returns one "not implemented" violation. `JunctionSpec`, the notation parser (gaps, multi-lane refusal) and `DeriveCurveSpacing` (the D67 table as cases). The direction table, the turn rule, the route rasterizer (`F/L/R/B<n>`), the union into a layout, and the ASCII renderer, each with inner tests. | API and geometry primitives; outer test red for the right reason |
| B | **Rule checker** (agent) | The route tracer and all of G1–G10. Every group gets fixtures built with A's route programs, each breaking only its rule. Curve fixtures are checked against hand-worked game-formula cases. | Full oracle |
| C | **Templates and reference layout** (orchestrator) | Design the route programs and variables in `TEMPLATES-1b.md`. Hand-build the L_R TL4/SD2 reference with A's tools until B's checker passes it, and show the engineer its ASCII. It becomes the fixture "reference layout is rule-clean". | Settled geometry, reviewed by the engineer |
| D | **Generator** (agent) | Built from `TEMPLATES-1b.md`: choices X1–X5, the slack sweep, scoring, caps and errors, then the signal and prio pass (§6). The outer test goes green and loses `[!mayfail]`. The sweep test (TL 3–6, SD 2–4, gaps 0–3 on both lines, both road sides) requires every result to be rule-clean or fail with a named error. The render test writes the layouts. | Generator done |
| E | **Game side** (agent) | Settings, `MakeJunctionSpec`, the reference speed, the bridge predicate, road-side mirroring, the window controls and status line, the estimate cache and strings. Tests wherever a seam exists. | Working in game |
| F | **Manual checks** (engineer) | Below. | Accepted |

## Manual checks (step F)

1. **Driving side** on screen is right-hand in a right-hand game, and left-hand after switching road side. This confirms §1's derivation.
2. **Generation:**
   - The ghost of the generated T follows the cursor.
   - Changing TL or a notation regenerates it.
   - A multi-lane notation shows the message.
   - The price matches the money spent.
3. **Build and run trains through all six movements** at the reference speed. No train slows for curves (watch the speed in the vehicle window).
4. **The prio holds a joining train** while an ML train is within TL upstream. Joiners go when the ML is clear. With signal type = path, run the six mixed-block checks in `RECON-path-prio.md`.
5. **No ML blocking:** a queue on the outbound branch never backs up onto the ML (G7).
6. **No deadlock** after 30 minutes of game time at heavy traffic.
7. **Save compatibility (D69):** save with the patched build and load in a stock build of the same version, and the reverse. Also check `openttd.cfg` keeps the junction keys after a stock build has run.
8. **Hover performance** over a large layout feels smooth (D63). Generation is under about 200 ms (D62).

## Risks

- **Template design (step 5) is the crux.** If closed-form minimums often fail the checker, the slack sweep grows. The fallback is a greedy fix-up driven by violations, which is a decision for step C, not for an agent.
- **Mixed path/pre-signal blocks** are untested in game (D74 fallback).
- **G3's part-by-part model assumes full-length wagons.** 1c's picked train supplies real lengths.
- **The `SDTC_SSTR` string setting in `gui_settings.ini`** has no precedent there. Step E confirms the template works or falls back to two small enum/int settings.
