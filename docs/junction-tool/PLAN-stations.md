# Station generator: plan (v1, ro-ro drop)

**Rests on:** D106 (scope), `RULES-stations.md` (the coop rules S1–S63, P1–P15), and the junction tool's components.

## v1 product

A "Station" tool on the rail toolbar, next to Junction. The player sets:
- the platform count (a stepper, 2–8; RS: minimum 2);
- TL, or a picked train (D101);
- the line notation of the sideline (L_R);
- orientation (rotate and mirror).

The ghost follows the cursor, as the junction ghost does: two tiers, a library and a fit test (D96). Click builds it.

**Shape:** a drive-through (ro-ro) drop station.
- The incoming sideline stub splits into N parallel platforms, each exactly TL long.
- The platforms merge back into the outgoing sideline stub.
- The station sits between the two stubs.
- The stubs end where the player, or later the joiner and junction tools, connects to the ML.

## Reused as-is

| Component | Use |
|---|---|
| `FindJunctionRoute` (curve grammar, windows, fences, terrain) | route the entry split fan and the exit merge fan from the stubs to the platform ends |
| `PlaceJunctionSignals` (drag fill D97, bridge signals, splits and merges) | signal the fans; a new phase adds the platform exit signals |
| Checker framework (`CheckJunctionRules`, paths, blocks) | new station rule set; connectivity uses station ports (in stub, out stub, platforms) |
| Terrain snapshot, plane rule, Spannable (D78, D104) | fit on real ground |
| Library and fit test, worker, hover cache (D96) | instant ghost |
| Rig (`traffic`, waypoints, `orders_via`) | drop traffic: trains with an unload order at the station |

## New pieces

1. **Layout:** `JunctionLayout` gains station tiles (a rect, an axis, the platform count and length) and their JSON export.
2. **Station rules** (checker), from RULES-stations:
   - every platform is reachable from the in stub and reaches the out stub;
   - platforms are exactly TL long and parallel;
   - there's a signal before each split (entry) and at each platform exit (one-way);
   - merges have no prio needed for drop stations (S-rules on exits); confirm against `RULES-stations.md` §4;
   - the incoming line never slows: at least TL of straight braking space between the stub and the first split (S-rule on entry);
   - curve rules as for junctions.
3. **Generator:** `GenerateStation(spec)`. It places the platform block at a fixed canonical spot, routes each platform's entry and exit with fences and windows (the same planarity-order idea as D90), and sweeps the fan length and its spacing. Score: cost, then open space, then area.
4. **Game side:** build the station tiles (`CmdBuildRailStation`, one call for the block), estimate and price them, and draw the ghost (a station sprite or an outline).
5. **Window:** a Station window cloned from the Junction window (notation, TL, pick train, platforms stepper, rotate/mirror, status, cost).
6. **Rig:** a drop traffic test with N trains looping between a pickup source (a plain station far away) and the generated drop station. Measure the platform waiting heatmap and ML-side stops.

## Steps

| Step | What | Depends on |
|---|---|---|
| ST2 | Station rules in the checker, plus a hand-built fixture: an outer test for a known-good 2-platform ro-ro | n/a |
| ST3 | `GenerateStation`, the pure generator, for 2–4 platforms on flat ground | ST2 |
| ST4 | Station tiles in the layout, the game-side build and pricing, the JSON export, and the rig build | ST3 |
| ST5 | The Station window: ghost, hover worker, library | ST4, D96 |
| ST6 | Rig drop traffic test | ST4 |
| ST7 | Attach to an existing line end (D106, staged) | ST5 |
| Later | pickup stations (entry check, bays); terminus; balanced multi-line entries | n/a |

## Open (for the engineer, from RULES-stations, not blocking ST2–ST3)

- Entry style for v1: buffered (waiting bays) or direct? Drop stations need no entry check, so direct is the default unless told otherwise.
- Whether we may derive our own numbers (braking space, signal gaps) from rig tests.
