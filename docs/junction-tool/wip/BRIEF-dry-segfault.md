# Step: the PLANAR_DRY segfault on LL², pinned by AddressSanitizer, then fixed test-first

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Never use git stash, never start a game client, never touch ports 4077/4079 or running openttd processes. One heavy job at a time: build with `-j8` at most, run ONE sanitizer process at a time, never several in parallel (the machine has crashed under parallel ASan load). Time box 120 minutes.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`, branch `wt-planar`, clean at `e4b50580e1`. Release build `build-rel`.
- Known: `PLANAR_DRY=1 build-rel/openttd_test "planar components: LL² from its topology text (mapped) at the probe's area" -s` (builds every LL² variant without solving and prints a `DRY <tag> | ... | built` or failure line per variant) died with SIGSEGV part way through the listing, twice, on 2026-11-18. The machine also has a known CPU instability, so a crash counts only if a sanitizer or a repeat confirms it.
- Read first: `/home/pete/OpenTTD/CODINGSTYLE.md` (C++ part); `src/junction/planar/CLAUDE.md` ("Component solver", the PLANAR_DRY sentence); `src/junction/CLAUDE.md` "Working rules for agents".

## Steps
1. Reproduce in Release: run the command above once, output to a file; record whether it segfaults and the last DRY line.
2. ASan build in a new dir `/home/pete/OpenTTD/.claude/ng1/build-asan`: configure like build-rel (read `build-rel/CMakeCache.txt` for the generator and options) with `-DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address`; build only `openttd_test`, `-j8`. Run the same command once with `ASAN_OPTIONS=detect_leaks=0`; save the full report to a file under `/tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/dry-asan/`.
3. If ASan reports nothing and step 1 did not crash: stop and report (hardware suspicion; no fix).
4. If ASan names the fault: read the code at the frames it names and find the cause. If the fix is local and obvious (an out-of-range index, a dangling reference, a missing guard on a failed build path), do it test-first: a `[junction][planarcomponents]` test that drives exactly the faulting path with the smallest input you can find (red: a crash or wrong value under build-asan, recorded), then the fix, then green under build-asan and build-rel. If the fix would change behaviour beyond removing the fault, or needs a choice, stop and report the ASan frames and your reading.

## Gates (if a fix lands)
- `/home/pete/OpenTTD/tools/junction/jt gate build-rel` (count, time; budget 30 s, it ran ~30.1 s before, a known overrun not yours).
- The PLANAR_DRY command under build-rel completes (count the DRY lines, report built vs failed).
- Commit on wt-planar: "Fix: Junction planar <what>", ending "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>". Update `src/junction/planar/CLAUDE.md` only if a stated fact changes.

## Report
References read; reproduction result; ASan frames (top 6, file:line); cause; commit hash or stop reason; red evidence; gate results; `git status --porcelain` (build-asan is untracked: leave it, say so) and `git log -1`. When you have reported, stop.
