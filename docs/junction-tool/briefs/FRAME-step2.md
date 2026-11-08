# Brief FRAME step 2: the frame grows with the prio coverage

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes.
- **Step 1** (c6e1f6f85b) put every frame constant in `JunctionFrame` / `MakeJunctionFrame(spec)`, which still returns the defaults.
- **Read first:** `docs/junction-tool/RECON-frame.md`, with its parameter proposal and risks, and D108 and D109 in `DECISIONS.md`.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/frame2/`
- **Don't run any `tools/ottd` command.**

## 2. Changes

1. **`MakeJunctionFrame(spec)`.** With `dC = spec.tl * (spec.prio_tl − 1)`, which is 0 at prio 1:
   - `hub_x = 30 + dC`
   - `width = 64 + 2·dC`
   - `reach_east = 16 + dC`
   - `terrain_height`: unchanged at 43
   - `terrain_origin_y`: unchanged at −2
   At prio 1 everything equals the defaults, so the golden output must be identical.
2. **Router effort:** `base.max_expansions = 50000 * frame.width / 64`, rounded down.
3. **The library carries its frame.**
   - `BuildJunctionLibrary` returns a struct, or adds a field (choose the smaller change and say which), that includes the `JunctionFrame` it was built with.
   - `FitJunctionLibrary` uses that frame instead of `JunctionFrame{}`.
   - `FitJunctionLibraryNear` and the GUI instant tier (`junction_gui.cpp`, about l.469) build the terrain snapshot with the library's frame.
   - The refine tier already uses `MakeJunctionFrame(job->spec)`. Make sure the two agree; they do if both derive from the same spec.
4. **Callers of `MakeJunctionTerrain` with `JunctionFrame{}` that do have a spec available:** switch them to `MakeJunctionFrame(spec)`.

## 3. Tests

1. **Golden at prio 1:** `cmp` identical, before and after.
2. **New: "the frame grows with prio coverage":** `MakeJunctionFrame` at `prio_tl` 1 equals the defaults; at `prio_tl` 3 and TL4, `hub_x` is 38, `width` is 80 and `reach_east` is 24.
3. **New: "library fit uses the library's frame":** for a spec at `prio_tl` 2, `BuildJunctionLibrary` plus `FitJunctionLibrary` on a flat terrain built with that frame gives a fit, if the library isn't empty. If the library is empty at prio 2, that's acceptable here; WARN and report it.
4. **`[.gen]` counts at `JUNCTION_GEN_PRIO` 1, 2, 3 and 5.**
   - Report them, plus, for any count of 0, a failure-stage histogram: temporary prints at each `return std::nullopt` in `BuildJunctionCandidate`, removed before you commit, as in `briefs/1b-stepD3d-diag.md`.
   - Prio 2 and 3 may still fail on the bridge span (D109 is the next step). That's expected, not a stop.
5. Release suite, all passing.

## 4. Do not

- change routing, fences, windows (beyond the frame expressions from step 1), the signal pass or the checker
- implement D109 clustering (the next step)

## 5. Stop conditions

- The golden files differ at prio 1.
- Anything not covered.

## 6. Gates

1. `cmake --build build-rel --target openttd openttd_test > frame2/rel.log 2>&1`, then the Release suite, all passing.
2. `cmake --build build --target openttd openttd_test > frame2/dbg.log 2>&1`: no errors or warnings in the files you touched. Build only.

Commit message: `Change: Junction frame grows with the prio coverage`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 7. Report

- commit
- cmp result
- the library-frame change you chose
- `[.gen]` counts and failure histograms
- suite totals
- stops
- `git status --porcelain` and `git log -1`

When you have reported, stop.
