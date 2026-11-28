# cascade1: does a two-level cascade unlock T1's 13 remaining refused moves?

Setup as d527a: T1 hub, original = the loaded hub, allowed = its judge lines (none), rip on for the refused move M. The 13 moves are those of d527a with no unlocker. Level 1 (no unlocker) is taken from d527a, not rerun. Level 2: for each level-1 candidate N (attached far-end nodes, then end nodes of tracks the soft routes ran over, the same code path via a temporary seam) and each of 8 directions whose rip-off node move is refused, PlanarDragUnlockers(N move) with M's vertex filtered, then M retried with rip on U's hub. Temporary seam (counters, candidate export) in planar_drag.cpp and the probe in the test file, both reverted.

| # | move | 15 s | 15 s calls | 60 s | 60 s chain (U -> N -> M) | 60 s calls | 60 s seconds | 60 s end |
|---|---|---|---|---|---|---|---|---|
| 0 | node 0 (4,7) N | no | 283 | no | - | 627 | 36.2 | exhausted |
| 1 | node 0 (4,7) S | no | 291 | no | - | 627 | 36.2 | exhausted |
| 2 | node 1 (12,7) N | no | 279 | no | - | 1002 | 60.0 | budget out |
| 3 | node 1 (12,7) E | no | 291 | yes, depth 2 | (17,18)->(19,20); (16,3)->(18,5); M | 871 | 51.2 | |
| 4 | node 1 (12,7) S | no | 283 | yes, depth 2 | (21,24)->(19,22); (30,9)->(28,9); M | 558 | 29.8 | |
| 5 | node 3 (17,18) W | no | 257 | no | - | 562 | 37.7 | exhausted |
| 6 | node 4 (21,24) E | yes, depth 2 (4.3 s, 62 calls) | 62 | (not rerun) | (17,18)->(19,20); (16,3)->(18,5); M | | | |
| 7 | node 4 (21,24) W | no | 279 | no | - | 753 | 46.3 | exhausted |
| 8 | node 5 (30,9) N | no | 288 | yes, depth 2 | (16,3)->(18,1); (34,3)->(36,5); M | 997 | 56.8 | |
| 9 | node 5 (30,9) S | no | 289 | no | - | 1052 | 60.2 | budget out |
| 10 | node 7 (34,9) N | no | 199 | no | - | 1069 | 60.0 | budget out |
| 11 | node 7 (34,9) S | no | 202 | no | - | 670 | 39.8 | exhausted |
| 12 | node 7 (34,9) W | no | 199 | yes, depth 2 | (21,24)->(19,22); (30,9)->(28,9); M | 601 | 36.2 | |

Counts (13 moves): unlocked at depth 2 within 15 s: 1 (move 6). Within 60 s: 5 (3, 4, 6, 8, 12). Searched to exhaustion with no unlocker: 5 (0, 1, 5, 7, 11; 36 to 46 s each). Undecided at 60 s (budget out): 3 (2, 9, 10).

Time per PlanarDragTo call (every call the cascade made, including those inside PlanarDragUnlockers): about 0.054 s in the 15 s runs, 0.059 s in the 60 s runs (9389 calls in 550 s).

Paragraph: A depth-2 cascade unlocks 5 of the 13 moves, and all five are found only because the search is run to the end of a level-2 sweep, which costs 30 to 57 s (about 560 to 1000 calls) per move; at 15 s only the cheapest one is found. Five more are proved to have no depth-2 unlocker among the level-1 candidates (a legitimately blocked move, or one that needs a node outside the candidate set), and three need more than 60 s. Cost is the sweep size: candidates (2 to 6) x 8 directions of N x the unlockers' own candidate sweep (about 8 node moves each, each followed by a retry of N's move), at 0.055 s per call. To make it cheaper: (1) skip the N moves that are accepted or that do not touch the tracks in M's way, since the unlocker of an unlocker only matters if it frees N's own blocked tracks; (2) cache the level-2 results by (N, direction) because the same pairs recur across moves (the chains (17,18)->(19,20), (21,24)->(19,22), (16,3)->(18,5) and (30,9)->(28,9) each appear in two chains); (3) order the directions by how much they ease the blocker (towards M's target) and stop at the first hit rather than sweeping, which is already done here, so the time shown for unlocked moves is the time to the first hit; (4) cut the per-call cost, for instance with an earlier hard-pass reject. A depth-2 cascade is worth building as an interactive aid only with the first-hit stop and a cache, and as a background suggestion rather than a hover action; at the full sweep it is a 30 to 60 s operation. Note the sweep skips an N move that the rip-on drag would accept (the brief's "refused with rip false" case); those cases were not separately tried.
