# B0: baseline hover timings (GenerateJunctionNear) on the Buborough save

- Binary: .claude/play/build-play/openttd, head 2f177a9c6c (Release). Save: Buborough Bay Transport, 2243-02-19.sav, map 512x1024 (tile = y*512+x).
- Rig: ports 3991/3993, no GameScript reply needed (company_set timed out, junction_export is a console command and ran regardless).
- Call: junction_export <tile> <rot> <mirror> <tmp> L_R L_R 4 1 train=14194,1090000,561,25,196 prio=14; seed 1, n=200, uniform tile/rot/mirror.
- Size check: x=0 and x=511 tile indices (51200, 51711, 51712) answer invalid-tile, which only holds for W=512; 1024x512 would make them interior.
- Rig overhead: median of 20 'echo x' rcon calls = 107.9 ms (each call starts a Python process and an admin connection). Nothing subtracted.
- Run time: 21.6 s for 200 calls. Another agent may have been building (timings could be inflated 20-50 %).

## Outcomes

- error NoLayout: 198
- invalid-tile: 2

## Wall time

- overall: p50 107.9 / p90 109.3 / p99 126.6 / max 127.8 ms (n=200)
- error NoLayout: p50 107.9 / p90 109.3 / p99 126.6 / max 127.8 ms (n=198)
- invalid-tile: p50 89.2 / p90 89.2 / p99 89.2 / max 89.2 ms (n=2)
- share over 100 ms: 198 of 200 (99 %); the rig overhead alone is 108 ms, so this share carries no information.

## 10 slowest anchors

tile (x,y) | rot | mirror | outcome | ms
--- | --- | --- | --- | ---
248,827 | 3 | 0 | error NoLayout | 127.8
458,798 | 2 | 1 | error NoLayout | 126.6
137,129 | 2 | 0 | error NoLayout | 126.3
507,920 | 3 | 1 | error NoLayout | 114.4
149,72 | 1 | 1 | error NoLayout | 109.9
225,41 | 1 | 1 | error NoLayout | 109.8
393,119 | 2 | 1 | error NoLayout | 109.8
365,662 | 0 | 0 | error NoLayout | 109.7
506,905 | 2 | 1 | error NoLayout | 109.7
75,20 | 2 | 1 | error NoLayout | 109.6

## Finding

Every valid call returned NoLayout in about the rig overhead, i.e. the search ends almost at once at all 198 random spots; no spot produced a layout, so no hover search cost was measured. The two invalid-tile replies are map-edge tiles. Wall time via rcon cannot resolve anything under about 100 ms.
