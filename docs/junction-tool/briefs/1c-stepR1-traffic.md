# Brief 1c step R1: rig traffic run with a waiting heatmap

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`.
- **Why:** the engineer judges signalling by simulation ("it will all come out in simulation. It may just be down to waiting bay sizing"). D98 (f): rig traffic comes first. This step shows **where trains wait** in a generated junction under load. The engineer reads the result.
- **Read first:**
  - `docs/junction-tool/RECON-1c.md` §C, the rig tools;
  - `tools/ottd/README.md`;
  - `tools/ottd/ottd`: `cmd_junction_check`, `cmd_sample`, `layout_batches` and `main`'s `add(...)` registrations;
  - `tools/ottd/gs/main.nut`: the dispatch at l.132–166 and `CmdSample` (l.456);
  - `src/tests/junction_design.cpp`: the `[.circuit]` test (l.194 on) and how it writes its JSON.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/r1c/`

## 2. Changes

### 2.1 GS command `occupancy` (`gs/main.nut`)
- **Request:** `{cmd:"occupancy", vehicles:[ids], ticks:T, every:K, slow:S}`. Defaults: K = 4, S = 80 km/h.
- **Every K ticks**, for each vehicle:
  - read `GetCurrentSpeed` and the `GetLocation` tile;
  - if speed == 0, `stopped[tile]++`;
  - else if speed < S, `slow[tile]++`;
  - count stop events per vehicle (moving → 0) as `CmdSample` does.
- Sleep with `GSController.Sleep(K)` between samples.
- **Reply:** `{samples:N, stopped:[[x,y,n],...], slow:[[x,y,n],...], stops:[[id,count],...]}`.
- Use a GS table keyed by tile index, then convert it to lists.

### 2.2 `ottd traffic FILE (X Y | --find-clear) [--trains N=8] [--wagons W=3] [--warmup DAYS=20] [--ticks T=20000] [--every K=4]`
1. **Factor** the build, depot and train-start part of `cmd_junction_check` into `setup_circuit(adm, layout, a)`, which returns `(ox, oy, vids)`. `junction-check` calls it, with identical behaviour and output. `traffic` calls it too.
   - The `wagons` count, today a literal 3, becomes `a.wagons`, with a default of 3 for both commands.
   - Spacing between train starts stays as it is.
2. **Wait** `--warmup` game days, using the date ping as `junction-check` does, so the trains spread around the circuit.
3. **Run** `occupancy` with the layout-relative timeout pattern of `sample`: `timeout = 120 + ticks / 20` seconds or more. Check how `sample` sizes it and match it.
4. **Print a heatmap** over the layout. For each tile of the layout's bounding box (origin ox,oy, size from the JSON):
   - a digit 1–9 if the tile has stopped samples, scaled as `ceil(9·n / max_n)`;
   - `+` if it has only slow samples;
   - `#` if the JSON has track or a bridge head there;
   - `=` for bridge middle tiles;
   - `.` otherwise.
   Put x coordinates every 5 columns above, and the y coordinate at the start of each row. Coordinates are layout-relative.
5. **Print the top 10 stopped tiles,** layout-relative, with n and n/samples as a percentage, then the total stop events per train.
6. **End with the deadlock verdict:** reuse `junction-check`'s two-snapshot check, factored into a function used by both.
7. **Write** the raw reply plus the parameters to `RUNDIR/traffic-<unix time>.json`, and print that path.
8. Register it in `main`, and add a short section to `README.md`. Also add the missing `junction-here`, `junction-scan` and `traffic` entries there, with one line each.

### 2.3 Fresh circuit
The committed `layouts/circuit-LR-TL4.json` predates the D97 signal fill. Regenerate it with the `[.circuit]` test, using whatever env or path it takes, read from the test, on `build-rel/openttd_test`. Commit the new JSON. The test code doesn't change.

## 3. Run (print `join: build/openttd -n 127.0.0.1` when the server starts; don't wait for anyone)

1. `ottd stop`, `ottd start` (flat, not hilly), `ottd company`.
2. `ottd traffic tools/ottd/layouts/circuit-LR-TL4.json --find-clear --trains 6`.
3. Same with `--trains 12`.
4. Same with `--trains 20`.

Each run builds its own copy. `--find-clear` finds a fresh spot, so earlier copies don't interfere. If the map runs out of room, `ottd stop` and `ottd start` between runs.

## 4. Tests

- No C++ changes, so no Catch2 tests. Rig code has no test suite. Check it by running it.
- `junction-check` must still pass: `ottd junction-check tools/ottd/layouts/circuit-LR-TL4.json --find-clear` exits 0 (OK) once, after the refactor.

## 5. Do not

- change any C++ source or test
- change `sample`'s behaviour
- add orders or per-route spawning (a later step)
- interpret the heatmap beyond the numbers; the engineer reads it

## 6. Stop conditions

- The circuit deadlocks at 6 trains. Report the heatmap, then stop.
- The `[.circuit]` test fails with the current generator.
- Anything not covered.

## 7. Gates

1. The `junction-check` regression run passes.
2. The three traffic runs complete.

Commit message: `Add: Rig traffic run with a waiting heatmap`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 8. Report

- commit
- the three heatmaps verbatim, each with its top-10 list, stop events and verdict
- the layout's ASCII from `[.circuit]` (the junction part only, if it prints one), so the orchestrator can map tiles to roles
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
