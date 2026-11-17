# DIAG heap corruption in "planar: T3 builds from its mapped component layout" (2026-11-18)

Cause: `inline std::string g_tag;` (src/junction/planar/planar_model.h:127) is a process-wide global that
`TagScope` (planar_model.h:133-134) reads and writes. `SolvePlanarMapped` runs one `SolveMappedVariant` per thread
(planar_components.cpp:2781-2785), and every `MappedComposer::Build` / `Model::Cross|Seg|Deck|Tie` opens TagScopes
(planar_model.h:343,380,472,497; planar_components.cpp:1532,1562), so threads do unsynchronised
`std::string::operator=` and copy on the same string: a data race that frees the string buffer under another
thread's copy, i.e. heap corruption (also `Add` copies `g_tag` into `row.tag`, planar_model.h:301).

Evidence:
- ASan at 7073aacd56 (RelWithDebInfo, wt2 build-asan), first run: heap-use-after-free, READ size 23 in
  `TagScope::TagScope` (copy of g_tag into `old`) from `Model::Cross` in `MappedComposer::Build` (thread T12);
  freed by thread T1 in `TagScope::TagScope` (`g_tag = tag`, assign) from `MappedComposer::Build` (planar_components.cpp:1562).
- Predates the ladders and the face map: at 05e0159b09 (Release, 12 parallel runs) 2/12 abort with
  `tcache_thread_shutdown(): unaligned tcache chunk detected`. At 7073aacd56 Release: 1/12 abort (re-run of the 12).
- PLANAR_NOLADDER=1 at 7073aacd56: 0/12 aborts (fewer TagScopes and fewer allocations, the race window shrinks; not a fix).
- g_tag dates from 16217f4c5b; the threaded variants arrived later, so any threaded solve (SolvePlanar too, line 2715) is exposed.

Smallest fix (not committed): `inline thread_local std::string g_tag;`. Each variant runs wholly on its own thread
and TagScopes nest within one thread, so thread_local keeps the semantics. Verified in the throwaway worktree:
ASan build with that one-word change, test run 6 times in parallel: 6/6 pass, no report.
Logs: scratchpad/heap/ (asan.1.log, old.*.log, r7073.*.log, nl.*.log, fix.*.log).
