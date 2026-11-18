# Spike: slot layer on T2, run 3 (pre/post intervals, prio fray spacing)

Changes over run 2: every channel has `pre` and `post` (no-crossing channels have exactly those two); in/out ports pin `pre`/`post`; in the `post` of a channel split by a Fray site with `prio`, consecutive Line tracks need `s(right)-s(left) >= 2` (rows and copies keep 1; no Row sits between lines in this case). Model feasible for all three variants; objective 50 in each. Observed layouts are the run 2 dumps (solver unchanged). Script: /tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/slot-spike/cmp.py, output .../cmp.out. Turning lines (first piece heading differs from last piece heading) are excluded from totals; doubled lines (with a copy track) compare net signed step; others compare the sequence. "Model over / solver over" for non-doubled lines compare sum |step|; for doubled lines |net|.

Tables (pred = signed slot steps, link steps attached to both lines they join; obs = steps from the dump, length/2):

```
== o1t3f1s0
line  predicted  observed(turns)
A1   pred [] | obs [15, -8] | solver-over
A2   pred [] | obs [10, -5] | solver-over
AB1  pred [1] | obs [-2] | solver-over
AB2  pred [1] | obs [-1] | same-sum-diff-seq
AF1  pred [] | obs [] | equal
AF2  pred [] | obs [] | equal
B1   [doubled] pred net 0.0 (seq [1, -1]) | obs net 0.0 (seq [1, -1, 1, -1]) | equal
B2   [doubled] pred net -1.0 (seq [-1]) | obs net 0.0 (seq [1, -1]) | model-over
C1   pred [] | obs [9, -34] | solver-over
C2   pred [] | obs [6, -16] | solver-over
CB1  pred [] | obs [1] | solver-over
CB2  pred [-1] | obs [] | model-over
CD1  pred [] | obs [] | equal
CD2  pred [] | obs [] | equal
D1   EXCLUDED (turning) pred [] | obs [1]
D2   EXCLUDED (turning) pred [] | obs [19]
E1   pred [] | obs [] | equal
E2   pred [-1] | obs [-1, 1] | solver-over
ED1  pred [] | obs [2] | solver-over
ED2  pred [-1] | obs [-1, 2] | solver-over
EF1  pred [-1] | obs [-3] | solver-over
EF2  pred [] | obs [-2] | solver-over
F1   EXCLUDED (turning) pred [] | obs [-1]
F2   EXCLUDED (turning) pred [] | obs [-6]
compared=20 excluded=['D1', 'D2', 'F1', 'F2']
totals: equal=6 model-over=2 solver-over=11 same-sum-diff-seq=1
== o1t2f1s0
line  predicted  observed(turns)
A1   pred [] | obs [15, -8] | solver-over
A2   pred [] | obs [10, -5] | solver-over
AB1  pred [1] | obs [-2] | solver-over
AB2  pred [1] | obs [-1] | same-sum-diff-seq
AF1  pred [] | obs [1] | solver-over
AF2  pred [] | obs [1] | solver-over
B1   [doubled] pred net 0.0 (seq [1, -1]) | obs net 0.0 (seq [1, -1, 1, -1]) | equal
B2   [doubled] pred net -1.0 (seq [-1]) | obs net 0.0 (seq [1, -1]) | model-over
C1   EXCLUDED (turning) pred [] | obs [-35]
C2   EXCLUDED (turning) pred [] | obs [-17]
CB1  pred [] | obs [1] | solver-over
CB2  pred [-1] | obs [] | model-over
CD1  pred [] | obs [] | equal
CD2  pred [] | obs [] | equal
D1   pred [] | obs [-2, 1] | solver-over
D2   pred [] | obs [-5, 19] | solver-over
E1   pred [] | obs [] | equal
E2   pred [-1] | obs [-1, 1] | solver-over
ED1  pred [] | obs [2] | solver-over
ED2  pred [-1] | obs [-1, 2] | solver-over
EF1  pred [-1] | obs [-3] | solver-over
EF2  pred [] | obs [-2] | solver-over
F1   EXCLUDED (turning) pred [] | obs [-1]
F2   EXCLUDED (turning) pred [] | obs [-6]
compared=20 excluded=['C1', 'C2', 'F1', 'F2']
totals: equal=4 model-over=2 solver-over=13 same-sum-diff-seq=1
== o0t2f1s1
line  predicted  observed(turns)
A1   pred [] | obs [11, -9] | solver-over
A2   pred [] | obs [6, -6] | solver-over
AB1  pred [1] | obs [-2] | solver-over
AB2  pred [1] | obs [-1] | same-sum-diff-seq
AF1  pred [] | obs [1] | solver-over
AF2  pred [] | obs [1] | solver-over
B1   [doubled] pred net 0.0 (seq [1, -1]) | obs net 14.0 (seq [1, -2, 16, -1, 1, -1]) | solver-over
B2   [doubled] pred net -1.0 (seq [-1]) | obs net -2.0 (seq [-2, 1, -1]) | solver-over
C1   EXCLUDED (turning) pred [] | obs [-41]
C2   EXCLUDED (turning) pred [] | obs [-23]
CB1  pred [] | obs [1] | solver-over
CB2  pred [-1] | obs [] | model-over
CD1  pred [] | obs [] | equal
CD2  pred [] | obs [] | equal
D1   pred [] | obs [-12, 1] | solver-over
D2   pred [] | obs [-15, 19] | solver-over
E1   pred [] | obs [] | equal
E2   pred [-1] | obs [-1, 1] | solver-over
ED1  pred [] | obs [2] | solver-over
ED2  pred [-1] | obs [-1, 2] | solver-over
EF1  pred [-1] | obs [-5] | solver-over
EF2  pred [] | obs [-4] | solver-over
F1   EXCLUDED (turning) pred [] | obs [-1]
F2   EXCLUDED (turning) pred [] | obs [-6]
compared=20 excluded=['C1', 'C2', 'F1', 'F2']
totals: equal=3 model-over=1 solver-over=15 same-sum-diff-seq=1
```

Totals per variant (compared lines only):

| variant | compared | equal | model over | solver over | same sum, different sequence | excluded (turning) |
|---|---|---|---|---|---|---|
| o1t3f1s0 | 20 | 6 | 2 | 11 | 1 | D1 D2 F1 F2 |
| o1t2f1s0 | 20 | 4 | 2 | 13 | 1 | C1 C2 F1 F2 |
| o0t2f1s1 | 20 | 3 | 1 | 15 | 1 | C1 C2 F1 F2 |

Forced/optional: see the `slot` lines of the slots-*.txt files in the scratchpad dir (columns forced|optional and delta-objective); not re-tabulated here. Link steps are not pinned.

Rules read from code and open questions: unchanged from run 2 plus: the Fray `prio` spacing is applied only between consecutive Line tracks in `post` of `PlanarSite::channel` of a Fray site with `res.prio` (site.channel is the channel split); the composer's Fray gap is at planar_components.cpp:337 (`prio ? 4 : 2`). Open: the model still predicts almost no steps on the long strands (A, C, D, B1 in o0t2f1s1), where the solver's observed steps are route geometry; the AB/CB/E*/EF* predicted link steps have the right sign only sometimes (AB2 same sum, other sign).

## Run 2 summary

Run 2 added a charged (no equality) straight link across sites to run 1's model. T2 was feasible for all three variants (objective 36 for o1t3f1s0), but the model predicted nearly no steps: equal 4 / 2 / 2, model over 2 / 2 / 2, solver over 18 / 20 / 20 (o1t3f1s0 / o1t2f1s0 / o0t2f1s1) over all 24 lines, with the comparison counted per whole strand rather than per topology line.

## Orchestrator reading (2026-11-18)
- The per-line comparison counts channel-wide shifts and shape detours (for example C1 [9, -34], a heading wiggle 7-0-7-6-7) as steps. In the model, a channel's common offset is free by design, so those are not slot facts.
- The slot fact is the change in the gap between neighbouring lanes. Recomputed from cmp.out: 5 of 8 channel gap changes match in each of o1t3f1s0, o1t2f1s0 and o0t2f1s1.
  - The EF fray, the ED and CB join close-ups, CD and AF all match.
  - The misses are AB (solver +1, model 0) and B and E (model -1, solver 0).
- The misses all sit in the prio channels and their feeder. That is where today's solver uses the stagger with reserved rows, not D349's rows-as-tracks, so it cannot validate the prio part.
