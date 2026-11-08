# MX2: map commands in Release builds; PSG saves and the overflow export

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## State
- Work **only** in the worktree `/home/pete/OpenTTD/.claude/play`. Another agent is building in `/home/pete/OpenTTD`; don't touch that one.
- First run `git -C /home/pete/OpenTTD/.claude/play checkout --detach <head given in your launch prompt>`. Build in `build-play` (Release) only. Commit on detached HEAD and report the hash; the orchestrator cherry-picks it.
- MX1 (`briefs/MX1-map-export.md`, commit f36e816887 on the branch) added `map_export`, `map_export_station` and `map_hotspots`, registered inside the `#ifdef _DEBUG` block of `IConsoleDebugLibRegister` in `src/console_cmds.cpp`, so Release builds lack them. D124 needs them in Release.
- Scratchpad: `/tmp/claude-1000/-home-pete-OpenTTD/fbe4ce64-b3bc-43e9-8f0f-5ecd841b3986/scratchpad` (below: `$S`).

## Read first
- `CODINGSTYLE.md` (the console and naming parts).
- `src/console_cmds.cpp`: the three map commands, `IConsoleDebugLibRegister`, and `IConsoleStdLibRegister` (where Release commands are registered).
- `src/saveload/afterload.cpp` around line 703-723: a NewGRF mismatch only fails the load when `_networking`; a single-player load continues with the missing NewGRFs disabled and the game paused.

## Code change
- Move the registration of `map_export`, `map_export_station` and `map_hotspots` from the debug block into `IConsoleStdLibRegister`, next to related commands. The command functions themselves move out of any `#ifdef _DEBUG` region they sit in. Nothing else changes; the junction and station export commands stay debug-only.
- No new test: the existing `[mapexport]` tests cover the functions. Check the commands in the Release binary (below).

## Manual runs (required; report what you saw)
1. **Release check:** run a dedicated server on a copy of the Buborough save as MX1 did (load first, then pipe commands after a delay: `(sleep 12; printf 'pause\n...\n'; sleep N; printf 'quit\n') | build-play/openttd -D -c <cfg> -g <save copy>`), and run `map_hotspots 3`. Expect the same top window as MX1 (`192,408 to 239,455`, score 454.5).
2. **Overflow export:** in the same run, `map_export 55 900 90 935 $S/mapx/bub_overflow_71_917.json`. Report the tile count, and confirm the record for (71,917) has tracks X, Upper, Right and no signals.
3. **PSG saves without networking.** PSG_186 (`$S/psgsaves/dl/PSG_186.sav`) and PSG_219 failed in dedicated mode only because of `_networking`. Load each as a single-player game with no window, and run console commands in it. Find the supported way, in this order, and report which worked:
   - a. the null video driver (`-v null`, see `src/video/null_v.cpp` for its options) with a script executed at game start (look for how `game_start.scr` / `exec` scripts are found: `src/console.cpp`, `src/openttd.cpp`, `src/fileio*`);
   - b. any other existing start-up script hook you find in that code.
   Use a config/personal directory under `$S/mapx/psgrun/` so nothing touches the engineer's own config.
   Run `map_hotspots 10` and `map_export` for the top 3 windows of each save, writing `$S/mapx/psg186_<n>.json` and `$S/mapx/psg219_<n>.json`, plus the hotspot lines to `$S/mapx/psg186_hotspots.txt` / `psg219_hotspots.txt`.

## Do not
- Kill or restart running `build-play/openttd` processes (the engineer's client and the rig server; relinking is fine).
- Touch `/home/pete/OpenTTD` or the engineer's config and save directories (copy saves into the scratchpad).
- Change savegame format, game state, network code, or the load path (`afterload.cpp`).
- Use port 3979 if something is listening there (`pgrep -af openttd`, `ss -ltn`).

## Stop conditions
- No existing way to run console commands in a non-networked headless game: report what you tried, and stop after the Release change and runs 1-2 are committed and reported.
- A PSG save crashes the game: report the message and the backtrace head, and continue with the other.

## Gates
1. Build `build-play` targets `openttd` and `openttd_test`, output to files. No errors and no new warnings.
2. `build-play/openttd_test "[mapexport]"` passes.
3. Commit: `Change: Junction map export console commands are available in release builds`, with the body ending `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report
- References read
- Commit hash in the play worktree
- What moved (names only)
- Release check, overflow export, and PSG results (method that worked, hotspot lines, files written, what the game did about missing NewGRFs)
- Stops, and anything left undone
- State: `git -C /home/pete/OpenTTD/.claude/play status --porcelain` and `log -1`

When you have reported, stop.
