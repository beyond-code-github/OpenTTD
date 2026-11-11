# DIAG-near: why the in-game hover search returns NoLayout (B0b)

Binary: Release at 46d634055c plus scratch prints (not committed). Rig: new flat 64x64, tile 2080, L_R L_R, tl 4, prio=14, near 1 unless stated.

## Q1. near 1 on flat land returns NoLayout in ~19 ms
- Return path: `GenerateJunctionNear` computes the flat T map `ConstructJunctionTMap(spec, nullptr, nullopt, false)` (src/junction_game.cpp:421-425). It has no layout, so it returns NoLayout at src/junction_game.cpp:427-435, before any offset or the snapshot is used. The snapshot margin, the offsets and the cancel flag play no part. The GUI hover takes the same road (src/junction_gui.cpp:676-677).
- Evidence (`debug_level misc=6`): `junction T map at offset 0,0: flat map: height 11: A->S failed / height 12: N->A failed / height 13: N->A failed / height 14: no valid skeleton`. Heights stop at g+13 (src/junction_generator.cpp:1096).
- The flat map is fragile in the spec, not in the terrain. Same tile, same prio, only the design train's top speed changed (the reference speed): 100 ok, 120 ok, 140 NoLayout, 160 NoLayout, 180 ok, 196 NoLayout (near 1 gen_us 14-52 ms; near 0 builds at 196).
- Scratch unit test (not committed, ran `MakeTestJunctionSpec` variants through the flat map): the test spec builds at height 13; reference_speed 196 fails; rail_piece 75 instead of 100 fails (B->S); bridge prices 800+200n instead of 500+200n fails; adding a tunnel price changes nothing; raising the 50000*width/64 expansion cap 100x changes nothing. So the unit tests pass only because the test spec's prices and speed are a lucky point: the old router routes edges one after another by cost, and a cheaper first route blocks the next edge.
- Fix (described): the flat map must not be a hard precondition that returns NoLayout. Either make the flat map price- and speed-insensitive (route it with the canonical prices, since it ignores terrain and only gives ports and a first height), or when it fails fall back to the sweep (`GenerateJunction`'s candidate sweep) instead of returning. Also add a test that runs the flat map over the real `MakeJunctionSpec` outputs (rail 75/2143, bridges, speeds 100-196).
- Side bug: `names[]` in src/console_cmds.cpp:383 has no "PortBlocked" entry (enum index 7), so the export prints garbage for it (seen: a file path as the error name). Add the name.

## Q2. near 0 on Buborough fails in 36-150 ms
- It is the terrain, not a bug. Snapshot at (300,600), margin 0 (84x43 cells): buildable 1253, water 2244, spannable 41, blocked 74, planes 1 (known from `JunctionTerrainPlanes`, src/junction_generator.cpp:1766). Over 60 percent water, so no plane carries the T. At (200,300) and (350,200) there are no planes at all and it returns NoLayout in 0 ms (src/junction_generator.cpp:1863).
- Wider check: near 0 at speed 120 (where flat land builds in 32 ms), 308 grid tiles over the save: 0 ok. Even all-buildable windows (e.g. 165,704: buildable 3612 of 3612, no water, z 1..14) fail in 35-55 ms: the save is hilly (z span 13-17 inside one 84x43 window).
- Near 1 at that hilly tile with a speed (120) where the flat map works logs `port approach blocked at 52,0` (D179) or `S port edge failed` / `A failed` at height 11 at every offset, 17 offsets in 18 ms: the ports and edges are blocked by slope.
- Which check of the sweep fails first is not logged (`GenerateJunction` has no log; no new profiling). A trace would need a log line per candidate; not added.

## Q3. Where the 957 ms of flat near 0 goes (196 km/h)
- No timing fields exist for `GenerateJunction`; `JunctionTMapResult` has them only for the T map. From the code:
- The sweep `GenerateJunctionUnchecked` (src/junction_generator.cpp:1855-1912) builds `AddJunctionSweepParams` candidates (100 per height: 2 crossing costs x (48 symmetric + 2 parallelogram)) at heights ml_gap+5..ml_gap+20, each with a tight build and, if unclean, a loose build plus the rules check, on up to 8 threads.
- The sweep continues until two heights past the first clean one (line 1906). At 196 the first clean height is high (result 41x17), so about 12-14 heights x 100 candidates are built; at 120 the result is 42x12 and the same call takes 32 ms.
- The prio timing is skipped here (prio= makes it non-strict), and the signal cleanup and scoring are small. The T map, which is the cheap path, takes 14-50 ms for the same spec.
