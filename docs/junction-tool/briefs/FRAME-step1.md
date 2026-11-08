# Brief FRAME step 1: one frame struct for every fixed frame constant (pure refactor)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## 1. State

- **Repo:** `/home/pete/OpenTTD`, branch `feature/junction-tool`, head `{HEAD}`. The Release suite passes.
- **Read first:** `docs/junction-tool/RECON-frame.md`, all of it. It lists every site (file:line) and proposes this step as "commit 1".
- **This step changes no behaviour.** `[.golden]` must be byte-identical, and every test must pass unchanged, apart from mechanical signature updates.
- **Scratchpad:** `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad/frame1/`
- **Don't run any `tools/ottd` command.**

## 2. Changes

1. **In `junction_generator.h`:**

   ```cpp
   /** The canonical working frame the generator, terrain snapshot and library fit share (RECON-frame). */
   struct JunctionFrame {
   	int hub_x = 30; ///< Working x of the hub (N column x E row), xI.
   	int width = 64; ///< Working x runs 0 .. width-1.
   	int reach_east = 16; ///< The router's search box ends this far east of xO.
   	int terrain_height = 43; ///< Terrain rows, from terrain_origin_y.
   	int terrain_origin_y = -2; ///< Working y of the terrain's first row.
   };
   /** The frame for a spec. In this step it always returns the defaults above. */
   JunctionFrame MakeJunctionFrame(const JunctionSpec &spec);
   ```

2. **Replace every literal that RECON-frame lists** with the frame's fields, or with expressions of them:
   - 63 → `width − 1`
   - 61 → `width − 3`
   - 62 → `width − 2`
   - `xO + 16` → `xO + frame.reach_east`
   - and so on.

   Sites:
   - `BuildJunctionCandidate`: all sites, including the lane search loop bounds;
   - `FitJunctionLibrary` (`30 − hub` → `frame.hub_x − hub`);
   - `MakeJunctionTerrain` (width = `frame.width + 2·margin`, height = `frame.terrain_height + 2·margin`, origin = `(−margin, frame.terrain_origin_y − margin)`, and the 30 in its mapping → `frame.hub_x`).

   `BuildJunctionCandidate` computes `const JunctionFrame frame = MakeJunctionFrame(spec);` at the top.

3. **`MakeJunctionTerrain` gains a parameter, `const JunctionFrame &frame`,** placed before `margin`. Update every caller (`junction_gui.cpp`, `console_cmds.cpp`, `junction_game.cpp`, tests), passing `MakeJunctionFrame(spec)`. Where a caller has no spec, pass `JunctionFrame{}` and list each such place in your report.

4. **The generator's `hub` out-parameter** must use `frame.hub_x` where it hard-codes 30.

5. **Comments** that say "(30, 0)", "64×43" and so on: reword them to refer to the frame.

6. **Tests:** where fixtures hard-code 64, 43, −2 or 30 (RECON-frame's risk list), replace those with `JunctionFrame{}` fields. Keep the test meaning.

## 3. Gates

1. **Golden:** `[.golden]` before the change (`frame1/before.txt`) and after (`frame1/after.txt`), then `cmp`. They must be identical.
2. Release build and suite, all passing.
3. `cmake --build build --target openttd openttd_test > frame1/dbg.log 2>&1`: no errors or warnings in the files you touched. Build only.
4. Grep the touched files for any remaining bare `63`, `61`, `62`, `+ 16` or `30` used as a frame value, and list any you deliberately left, with the reason.

Commit message: `Codechange: Junction frame constants come from one struct`. Signing is off; end the message with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## 4. Do not

- change any value (this is step 1, defaults only)
- touch the router, the signal pass or the checker

## 5. Stop conditions

- The golden files differ.
- A site in RECON-frame turns out not to be a frame constant, or a frame constant isn't in RECON-frame. Report it; it can be handled after the stop.
- Anything not covered.

## 6. Report

- commit
- cmp result
- every replaced site, briefly
- leftovers and the callers using `JunctionFrame{}`
- suite totals
- `git status --porcelain` and `git log -1`

When you have reported, stop.
