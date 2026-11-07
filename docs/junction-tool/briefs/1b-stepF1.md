# Brief 1b step F1: test circuit and `ottd junction-check`

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`.
- **Rig:**
  - `tools/ottd/`: CLI `ottd`, GameScript `gs/main.nut`, `README.md` (commands, layout JSON, gotchas).
  - It builds layouts with `build-layout FILE X Y` or `--find-clear`, and has depots, trains and speed sampling.
  - The IdleAI company runs out of cash after about two junctions.
- **Layouts:** `GenerateJunction(MakeTestJunctionSpec())` writes to `tools/ottd/layouts/generated-LR-TL4.json` through the hidden `[.gen]` test in `src/tests/junction_design.cpp` (`JUNCTION_JSON`, `JUNCTION_MIRROR`, `WriteLayoutJson`).
- **The engineer watches rig runs:** print the join command `build/openttd-spike -n 127.0.0.1` when you start the server. Don't wait.
- **Another agent** works in `/home/pete/OpenTTD-terrain` (separate worktree). Don't touch it.

## 2. Read first

- `tools/ottd/README.md`
- `tools/ottd/ottd`: argument parsing, `build-layout`, `train`, `sample`
- `tools/ottd/gs/main.nut`: the command dispatch, `build_layout`, `build_depot`, `build_train`, `sample`
- `src/tests/junction_design.cpp`: the `[.gen]` test and `WriteLayoutJson`
- `src/junction_router.h`, and `src/junction_rules.h` (`FindJunctionPorts`, `JunctionPort`)

## 3. Changes

### 3.1 The circuit (C++, hidden test `[.circuit]` in `src/tests/junction_design.cpp`)

Start from `GenerateJunction(MakeTestJunctionSpec())`'s layout (canonical frame, normalised) and add a turnaround loop beyond each of its three leg ends.

**Ports:** `FindJunctionPorts(layout)` gives the six ports: EIn/WOut on the west edge, EOut/WIn on the east edge, NIn/SOut on the bottom edge.

**For each end, route a loop with `FindJunctionRoute`:**
- start: the "out" port's piece (WOut, EOut, SOut), leaving the layout;
- goal: the "in" port's piece (EIn, WIn, NIn), entering the layout;
- bounds: a box beyond that edge only, 1 tile of margin from the layout's bounding box, 24 tiles deep and as wide as the layout plus 24 on each side;
- spec: `MakeTestJunctionSpec()` (CL, s_bend_window = 2·tl, prices, `bridge_span_ok`).

`AddRasterToLayout` each loop. If a loop fails, `FAIL` with the end's name.

**Loop signals:** along each loop's raster, put a one-way `SignalType::Block` signal facing along travel. Start 2 pieces after the start, then add one every time 2·sd units have passed, only on pieces where a signal is legal: not a bridge piece, and the tile holds exactly one track. No signals on the first and last piece.

**Depot:**
- Pre-lay one axis track tile T, 3 tiles further out from the middle of the west loop's outermost straight. Choose T by scanning that loop's pieces for the axis piece farthest from the layout, then stepping 3 tiles outward perpendicular to the edge (−x for the west end).
- Route a spur from the loop's pieces to T with `FindJunctionRoute`: starts = every axis piece of the west loop; goal = T with the trackdir pointing away from the loop.
- The depot sits on the tile beyond T, with its entrance facing back toward T.

**Normalise** at the end, keeping the depot coordinates in the same offset.

**Output:**
- `JUNCTION_CIRCUIT=<path>` writes the JSON with `WriteLayoutJson` plus a top-level field `"depots":[{"x":..,"y":..,"dir":"NE|SE|SW|NW"}]`. Use the rig's direction names (the way the entrance faces).
- If the existing writer can't take an extra field cleanly, extend `WriteLayoutJson` with an optional depots parameter.
- Write `tools/ottd/layouts/circuit-LR-TL4.json`, and print the ASCII.

### 3.2 GameScript (`gs/main.nut`)
- **New command `loan_max`:** in company mode, `ScriptCompany.SetLoanAmount(ScriptCompany.GetMaxLoanAmount())`. Reply with the bank balance.
- **The `vehicles` reply:** add the tile (x, y) of each train, if it is not already there.

### 3.3 Rig config (`ottd_core/server.py`, where `openttd.cfg` is written)
- Set `[difficulty] max_loan` to the highest value the setting allows. Look up its range in `src/table/settings/difficulty_settings.ini`, or wherever it is defined, and use that maximum.
- If the config writer can't express this, stop.

### 3.4 CLI: `ottd junction-check FILE [--trains N] [--ticks T] [--find-clear | X Y]`
Defaults: N = 6, T = 7400 (about 100 game days).

1. **Loan:** `loan_max`.
2. **Build:** `build-layout` of FILE. Abort on failures.
3. **Depots:** build each depot from FILE's `depots`, offset by the origin.
4. **Trains:** buy N trains in the first depot (default engine, wagons 3) and start each one about 300 ticks after the previous one. Use the existing `build_train`/`start`; poll the date to space them. No orders: trains wander, which drives all six movements.
5. **Sample:** run `sample` on all trains for T ticks.
6. **Report** one line per train (avg, min, max, stops, drops), then a verdict:
   - `DEADLOCK?` if every train had speed 0 throughout the last 20% of the sample. If the sample command can't tell that, take a `vehicles` snapshot twice, 1000 ticks apart: every train has speed 0 at the same tile both times.
   - `OK` otherwise.

Exit code 0 for `OK`, 2 for `DEADLOCK?`, 1 for build failures.

### 3.5 Run it once
- `ottd start`, `ottd company`, then `junction-check tools/ottd/layouts/circuit-LR-TL4.json --find-clear`.
- Print the join command when the server starts.
- Report the full output. Leave the server running at the end.

### 3.6 Docs
- Add `junction-check`, `loan_max` and the `[.circuit]` test to `tools/ottd/README.md`.
- Add "the rig's junction-check passed" to `docs/junction-tool/CHECKS-1b.md`.

## 4. Do not

- change the generator, router, signal pass, checker or game code
- add orders logic beyond the above
- touch `/home/pete/OpenTTD-terrain`

## 5. Stop conditions

- A loop can't be routed within the stated bounds.
- A GameScript API named here doesn't exist in this version. Check `src/script/api/script_company.hpp`.
- The circuit builds with failures.
- Anything not covered.

## 6. Gates

1. `cmake --build build --target openttd openttd_test > /tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/f1_build.log 2>&1`. There must be no errors or warnings in the files you touched.
2. Full suite. The baseline is 213 cases, all passing.
3. The rig run in §3.5.

Commit message: `Add: Junction test circuit and rig junction-check`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 7. Report

- commit
- the circuit ASCII
- the `junction-check` output and verdict
- suite totals
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
