# G4 2b2b1b: band event rows, crossing rows rho, layer order and event-order binaries (DESIGN-G4 A3, §11.4; D273)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
O3 (2b2b2) guards each band row by the order of the band's events. This step records, as static rows, the rows of those events and their order:
- each band event has a row: a band node's point row, or, for a band crossing, a new integer variable rho;
- rho is tied to the crossing's geometry in each form (DESIGN-G4 §11.4): S-bend form `rho = y(north head of the single bridge) + 1`, square form `rho = y(the run's row)`;
- each layer pair of the arrangement (`band_pairs`: north strictly north of south) is an unconditional row;
- each incomparable neighbouring event pair (`band_event_pairs`, A3) gets one binary, its order, with one row per value.

Data only: nothing reads them in a solve yet; no layout, objective, hash or digest moves. RECON-O3 Q5 found the seeds' event order differs from the sweep's index order (LL_RR x L_R 4 of 7 pairs, L_R x LL_RR 3 of 4), which is why the order is a binary.

## State
- Worktree `/home/pete/OpenTTD/.claude/o4c2`: run `git -C /home/pete/OpenTTD/.claude/o4c2 checkout -B wt-2b2b1b <HEAD given at launch>`. Leave untracked files alone.
- `build-rel` (Release) exists. Delete stale `.pch` files if the build complains.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/2b2b1b/`.
- Build only `openttd_test`. Never start a game client.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md` (C++ part).
- `docs/junction-tool/DECISIONS.md`, rows D255, D260, D261, D273.
- `docs/junction-tool/DESIGN-G4-static.md` lines 54-70 (§1) and 357-379 (§11.4).
- `docs/junction-tool/RECON-O3-bands.md` (Method paragraph: how it read event rows; Q5).
- `src/junction/topology/junction_topology.h` lines 140-160 (`JunctionTrackBandEvent`, `order.bands`) and the arrangement (`band_pairs`, `band_event_pairs`).
- `src/junction/placement/junction_placement.h` lines 160-260 (`JunctionSystemFormGroup`, `JunctionStaticRow`, the system's members).
- `src/junction/placement/junction_placement.cpp`: where a crossing group's form is read (grep `form_groups`), the single bridge's heads of a band crossing (grep `north head` and the over rows near the old jp:1862-1866 comment "over rows put the heads one row beyond the diagonal"), the square form's run (grep `choices.run`), and the end of the item-row record block (where `static_rows` are pushed; grep `O1 item`).
- `src/tests/junction/topology/junction_topology.cpp`: `StaticRowValue` and the item-row seed helpers (grep `RecordItemRowSeed`).

## Changes
In `junction_placement.h`:
```cpp
/** A band event's row in the static model (DESIGN-G4 A3): a band node's point row, or a band crossing's rho. */
struct JunctionBandEventRow {
	uint32_t band;
	uint32_t event; ///< Index into order.bands[band].
	int pt; ///< A node: its point (the row is y[pt]); -1 for a crossing.
	int rho; ///< A crossing: index into JunctionPlacementSystem::rhos; -1 for a node.
};

/** A band crossing's row variable rho (DESIGN-G4 §11.4). */
struct JunctionCrossingRho {
	uint32_t crossing; ///< Index into graph.crossings.
	int group; ///< Index into form_groups of the crossed edge's crossing group.
	int value; ///< Its value at the recording: the S-bend form's north head row + 1, or the square form's run row.
};

/** An order binary of two incomparable band events (DESIGN-G4 A3): north_first means the first lies at or north of the second. */
struct JunctionEventOrder {
	uint32_t band;
	uint32_t first; ///< Index into order.bands[band].
	uint32_t second;
	bool north_first; ///< Its value at the recording.
};
```
`JunctionStaticRow` gains:
```cpp
	std::vector<std::pair<int, int>> rho; ///< (index of rhos, coefficient), each adding coefficient * rho to the left side.
	std::vector<std::pair<int, bool>> orders; ///< (index of event_orders, value): it holds only when each order has that value; empty: always.
	int group = -1; ///< With form: it holds only when form_groups[group] takes that form; -1: in both forms.
	int form = 0;
```
`JunctionPlacementSystem` gains `std::vector<JunctionCrossingRho> rhos;`, `std::vector<JunctionBandEventRow> band_event_rows;` (per band, per event, flattened in band then event order) and `std::vector<JunctionEventOrder> event_orders;` (in `band_event_pairs` order).

A pure Y row is a `JunctionStaticRow` whose `v.pt` and `u.pt` are -1 (no x term; the existing convention) with `y` and `rho` terms and `gap` the constant. Write `Y(E)` for an event's row term: `y[pt]` or `rho`.

In `junction_placement.cpp`, in the record block after the item-row rows (record only; skip when the graph has no band events), push these static rows with a `reason` beginning "O3 event ":
1. **rho definition, per band crossing**, two rows per form (an equality as two `>=` rows), each with `group` and `form` set: S-bend form (0) `rho - y[north head] >= 1` and `y[north head] - rho >= -1`; square form (1) `rho - y[run row point] >= 0` and its opposite. The north head is the single bridge's head with the smaller row; the run row point is the point whose row is the square run's row (the first Corner's last point). If a crossing's form-1 run point does not exist in a recording made in form 0, stop and report how the recording names it (2a1 gave both forms points of their own; find them, do not invent one).
2. **Layer rows**, per `band_pairs` entry (band, south, north): `Y(south) - Y(north) >= 1`, always.
3. **Event orders**, per `band_event_pairs` entry: an order with `north_first = Y(first) <= Y(second)` at the recording, and two rows: under `{order, true}` `Y(second) - Y(first) >= 0`; under `{order, false}` `Y(first) - Y(second) >= 0`.

`StaticRowValue` in the tests evaluates `rho` terms from `rhos[i].value`. A row holds at the recording when its guards (pieces, fan pieces, orders, group form) are all as recorded and its value is at least its gap; a row whose guards are not all as recorded is skipped by the check.

## Tests (red first: declare the members first, so the red is an assertion)
In `src/tests/junction/topology/junction_topology.cpp`, after the O2 tests, tag `[junction][topology]`:
1. **"band events: every event row holds at the seeds"**: T, LL_RR x L_R, L_R x LL_RR recorded at their seeds: `rhos` has one entry per band crossing (1, 1, 3); every "O3 event" row whose guards are as recorded holds; each crossing's rho equals its S-bend form value (all three seeds are in form 0). Red: no rows.
2. **"band events: the seeds' orders against the sweep's index order"**: the number of `event_orders` is 1, 7 and 4; the number with `north_first == false` (the first event lies south of the second, against the index order, or level counted as north) equals RECON-O3 Q5's (T 0, LL_RR x L_R 4, L_R x LL_RR 3: the recon counted "first north of second" as against the order; read Q5 and use its exact definition; if the counts differ only by the definition, report it and pin the measured values with the recon's definition stated in the test's comment).
3. **"band events: one row per layer pair and per event"**: at the three seeds, every `band_pairs` entry has exactly one layer row, and every band event appears in `band_event_rows` exactly once.

## CLAUDE.md
`src/junction/placement/CLAUDE.md`, append to the Placement section: "A recording gives each band event a row (a band node's point row, or a band crossing's rho, tied in each form to the single bridge's north head row plus one or to the square run's row), states each layer pair of a band as a row (north at least one row north of south), and orders each incomparable neighbouring event pair by a binary with one row per value. The solver does not read them yet."

## Do not
- Do not read these rows in the joint model or the loop; do not change any existing row, test pin or recording size other than these new members.
- Do not add free-bend binaries or O3 band rows (2b2b2).
- Do not touch `src/junction/topology/` (another step, 2b2b1a, changes it in parallel).

## Stop conditions
- A layer row or an order row does not hold at a seed.
- A band event's node has no point in the recording, or a crossing's north head or run point cannot be found.
- Any existing test changes value.

## Gates
- While working: `tools/junction/jt fast topology` and `tools/junction/jt fast placement`, output to files.
- Before the commit: `tools/junction/jt gate`, `tools/junction/jt sweep`, `tools/junction/jt identity`, once each; report count and time.
- One commit: `Add: Junction recording states band event rows and their order`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`; the CLAUDE.md edit in the same commit.

## Report
References read; commit (hash, subject); red evidence; types and members added (names only); suite results (count, time, failures you did not cause); counts per seed (rhos, layer rows, orders, orders against index order); LL_RR x LL_RR not recorded; stops and anything left undone; state: `git status --porcelain` and `git log -1`. When you have reported, stop.
