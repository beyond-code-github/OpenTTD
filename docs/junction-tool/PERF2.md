# PERF2: GenerateJunctionNear wall time (Release clang-18, head 7b853cf7e5, median of 5)

| terrain | median ms | found offset | anchor total us | flat | route | skeleton | finish | check | heights | trace lines | relaxed | anchor built |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| a flat | 62.9 | 0,0 | 47424 | 32893 | 5489 | 50 | 532 | 8384 | 1 | 15 | 0 | yes |
| b G3c ridge | 84.2 | 0,0 | 88619 | 32773 | 34493 | 400 | 1014 | 19614 | 5 | 82 | 0 | yes |
| c town hill + tunnel price | 472.5 | 0,0 | 431173 | 21174 | 395347 | 235 | 977 | 12562 | 5 | 49 | 0 | yes |
| d town hill, no tunnel | 332.3 | -4,-4 | 23750 | 23265 | 358 | 0 | 0 | 0 | 3 | 6 | 2 | no |
| e obstacle beside hub | 39843 | -2,2 | 11883846 | 21228 | 11850859 | 0 | 0 | 0 | 33 | 165 | 0 | no |
| f field of obstacles | 489.9 | 0,0 | 486917 | 21481 | 449071 | 104 | 1192 | 14942 | 3 | 36 | 0 | yes |

JunctionRelaxedReachable alone: skipped.
