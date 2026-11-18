# Spike: slot layer on T2, run 2 (straight link charged)

Model feasible for all three variants (objective 36 for o1t3f1s0). Tables below: predicted = signed slot steps per line (link steps across sites attached to both lines they join), observed = pieces of heading h+-1 between two pieces of heading h, in slots (length/2), from the PLANAR_MAPPED_DUMP seq lines. Script: /tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/slot-spike/cmp.py, output /tmp/claude-1000/-home-pete-OpenTTD/5aaa14e0-b8b8-4d2d-bc21-f25c3b5bedf1/scratchpad/slot-spike/cmp.out. Observed step sizes of 6 or more slots are long diagonal runs (route geometry), not sideways steps in the brief's sense, but are reported as the brief defines them.

```
== o1t3f1s0
line  predicted  observed(turns)
A1   pred [] sum=0 | obs [15, -8] sum=23.0 turns=1 | differ
A2   pred [] sum=0 | obs [10, -5] sum=15.0 turns=1 | differ
AB1  pred [] sum=0 | obs [-2] sum=2.0 turns=0 | differ
AB2  pred [] sum=0 | obs [-1] sum=1.0 turns=0 | differ
AF1  pred [] sum=0 | obs [] sum=0 turns=0 | equal
AF2  pred [] sum=0 | obs [] sum=0 turns=0 | equal
B1   pred [-1] sum=1.0 | obs [1, -1, 1, -1] sum=4.0 turns=3 | differ
B2   pred [-1] sum=1.0 | obs [1, -1] sum=2.0 turns=1 | differ
C1   pred [] sum=0 | obs [9, -34] sum=43.0 turns=1 | differ
C2   pred [] sum=0 | obs [6, -16] sum=22.0 turns=1 | differ
CB1  pred [] sum=0 | obs [1] sum=1.0 turns=0 | differ
CB2  pred [-1] sum=1.0 | obs [] sum=0 turns=0 | differ
CD1  pred [] sum=0 | obs [] sum=0 turns=0 | equal
CD2  pred [] sum=0 | obs [] sum=0 turns=0 | equal
D1   pred [] sum=0 | obs [1] sum=1.0 turns=1 | differ
D2   pred [] sum=0 | obs [19] sum=19.0 turns=1 | differ
E1   pred [1] sum=1.0 | obs [] sum=0 turns=0 | differ
E2   pred [1] sum=1.0 | obs [-1, 1] sum=2.0 turns=1 | differ
ED1  pred [1] sum=1.0 | obs [2] sum=2.0 turns=0 | differ
ED2  pred [] sum=0 | obs [-1, 2] sum=3.0 turns=1 | differ
EF1  pred [] sum=0 | obs [-3] sum=3.0 turns=0 | differ
EF2  pred [1] sum=1.0 | obs [-2] sum=2.0 turns=0 | differ
F1   pred [] sum=0 | obs [-1] sum=1.0 turns=1 | differ
F2   pred [] sum=0 | obs [-6] sum=6.0 turns=1 | differ
totals: equal(sequence)=4 model-over=2 solver-over=18 sameSum-diffSeq=0
== o1t2f1s0
line  predicted  observed(turns)
A1   pred [] sum=0 | obs [15, -8] sum=23.0 turns=1 | differ
A2   pred [] sum=0 | obs [10, -5] sum=15.0 turns=1 | differ
AB1  pred [] sum=0 | obs [-2] sum=2.0 turns=0 | differ
AB2  pred [] sum=0 | obs [-1] sum=1.0 turns=0 | differ
AF1  pred [] sum=0 | obs [1] sum=1.0 turns=0 | differ
AF2  pred [] sum=0 | obs [1] sum=1.0 turns=0 | differ
B1   pred [-1] sum=1.0 | obs [1, -1, 1, -1] sum=4.0 turns=3 | differ
B2   pred [-1] sum=1.0 | obs [1, -1] sum=2.0 turns=1 | differ
C1   pred [] sum=0 | obs [-35] sum=35.0 turns=1 | differ
C2   pred [] sum=0 | obs [-17] sum=17.0 turns=1 | differ
CB1  pred [] sum=0 | obs [1] sum=1.0 turns=0 | differ
CB2  pred [-1] sum=1.0 | obs [] sum=0 turns=0 | differ
CD1  pred [] sum=0 | obs [] sum=0 turns=0 | equal
CD2  pred [] sum=0 | obs [] sum=0 turns=0 | equal
D1   pred [] sum=0 | obs [-2, 1] sum=3.0 turns=1 | differ
D2   pred [] sum=0 | obs [-5, 19] sum=24.0 turns=1 | differ
E1   pred [1] sum=1.0 | obs [] sum=0 turns=0 | differ
E2   pred [1] sum=1.0 | obs [-1, 1] sum=2.0 turns=1 | differ
ED1  pred [1] sum=1.0 | obs [2] sum=2.0 turns=0 | differ
ED2  pred [] sum=0 | obs [-1, 2] sum=3.0 turns=1 | differ
EF1  pred [] sum=0 | obs [-3] sum=3.0 turns=0 | differ
EF2  pred [1] sum=1.0 | obs [-2] sum=2.0 turns=0 | differ
F1   pred [] sum=0 | obs [-1] sum=1.0 turns=1 | differ
F2   pred [] sum=0 | obs [-6] sum=6.0 turns=1 | differ
totals: equal(sequence)=2 model-over=2 solver-over=20 sameSum-diffSeq=0
== o0t2f1s1
line  predicted  observed(turns)
A1   pred [] sum=0 | obs [11, -9] sum=20.0 turns=1 | differ
A2   pred [] sum=0 | obs [6, -6] sum=12.0 turns=1 | differ
AB1  pred [] sum=0 | obs [-2] sum=2.0 turns=0 | differ
AB2  pred [] sum=0 | obs [-1] sum=1.0 turns=0 | differ
AF1  pred [] sum=0 | obs [1] sum=1.0 turns=0 | differ
AF2  pred [] sum=0 | obs [1] sum=1.0 turns=0 | differ
B1   pred [-1] sum=1.0 | obs [1, -2, 16, -1, 1, -1] sum=22.0 turns=3 | differ
B2   pred [-1] sum=1.0 | obs [-2, 1, -1] sum=4.0 turns=2 | differ
C1   pred [] sum=0 | obs [-41] sum=41.0 turns=1 | differ
C2   pred [] sum=0 | obs [-23] sum=23.0 turns=1 | differ
CB1  pred [] sum=0 | obs [1] sum=1.0 turns=0 | differ
CB2  pred [-1] sum=1.0 | obs [] sum=0 turns=0 | differ
CD1  pred [] sum=0 | obs [] sum=0 turns=0 | equal
CD2  pred [] sum=0 | obs [] sum=0 turns=0 | equal
D1   pred [] sum=0 | obs [-12, 1] sum=13.0 turns=1 | differ
D2   pred [] sum=0 | obs [-15, 19] sum=34.0 turns=1 | differ
E1   pred [1] sum=1.0 | obs [] sum=0 turns=0 | differ
E2   pred [1] sum=1.0 | obs [-1, 1] sum=2.0 turns=1 | differ
ED1  pred [1] sum=1.0 | obs [2] sum=2.0 turns=0 | differ
ED2  pred [] sum=0 | obs [-1, 2] sum=3.0 turns=1 | differ
EF1  pred [] sum=0 | obs [-5] sum=5.0 turns=0 | differ
EF2  pred [1] sum=1.0 | obs [-4] sum=4.0 turns=0 | differ
F1   pred [] sum=0 | obs [-1] sum=1.0 turns=1 | differ
F2   pred [] sum=0 | obs [-6] sum=6.0 turns=1 | differ
totals: equal(sequence)=2 model-over=2 solver-over=20 sameSum-diffSeq=0
```

Forced/optional: only one nonzero interval step exists (B1 into post, -1): optional, objective change 0. Link steps are not pinned (brief item 6 covers line/interval pairs only).

Predicted link steps (o1t3f1s0): EF2->E2 +1, B2->CB2 -1, E1->ED1 +1; others 0. Offsets o(channel) hit the +-500 bounds (free translation), so only differences are meaningful.

Rules read from code: unchanged from run 1 (SPIKE-slots-run1.md). The only model change is the straight link: step variables up/dn, cost 1 each, no equality.

Open questions: the comparison counts observed steps per whole strand while the model is per topology line; strands spanning several lines (A, C, D, F, B) accumulate steps the model assigns to other lines. Not resolved here.
