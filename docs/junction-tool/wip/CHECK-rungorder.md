# Check: chord drawing vs solved layout, order of events across each corridor (2026-11-17, HEAD 05e0159b09)

Probe: wip/probe-corridors.patch plus a throwaway block (chord points stored in the topology, order check after the solve, env PLANAR_RUNGORDER=1, one variant by PLANAR_ONLY). Reverted. Raw output was in the scratchpad `rungorder/`.
Method notes: a face walk may start mid-wall, so each wall's stretches were rotated to the chain that joins end vertex to start vertex (0 join mismatches after that). Shared s and t crossings are the axis ends and are left out of the sequence; axis = mean of the two walls' first crossing vertices to mean of the last.

| hub variant | corridors | with a cross-wall order | agree | disagree | interior nodes left out | bends total (max per corridor) | stretches with no usable segment range |
| T2 o0t2f1s1 | 18 | 9 | 8 | 1 | 0 | 81 (10) | 2 |
| T3 o0t1f1s1 | 45 | 30 | 22 | 8 | 0 | 155 (12) | 4 |
| LL2 o49t0f1s0 | 24 | 6 | 5 | 1 | 18 | 138 (18) | 4 |
| LL2 o1t3f0s0 | 24 | 6 | 5 | 1 | 18 | 106 (12) | 0 |

Every checked corridor has exactly one interior crossing on each wall (sequence length 2: "LR" or "RL"): a diamond between s and t. No corridor has a longer interleaving, so the question is one binary order per diamond.
Disagreements (chord / layout), all diamonds:
- T2: face 27 (L=D1#4 F1#2, R=F2#1 D2#3) RL / LR.
- T3: face 19 (A2#0 B1#1 | B2#0 A3#1) RL/LR; 28 (F2#6 B1#4 | B2#3 F3#5) LR/RL; 37 (B1#9 C2#2 | C3#3 B2#8) LR/RL; 42 (B2#5 D2#3 | D3#2 B3#4) RL/LR; 50 (C2#0 D1#1 | D2#0 C3#1) RL/LR; 52 (D1#6 F2#2 | F3#1 D2#5) RL/LR; 54 (D1#7 F1#3 | F2#2 D2#6) RL/LR; 60 (D2#5 F2#3 | F3#2 D3#4) RL/LR.
- LL2 o49t0f1s0 and o1t3f0s0: face 16 (D1#4 F1#2 | F2#1 D2#3) RL/LR in both.
Reading: the two middle vertices of a diamond sit almost level along s-t, so the chord drawing's order is a near-tie and not evidence either way; the layout then picks. The same D/F, B/C, B/F pairings recur, and the layout's order was identical in both LL2 variants. Bends: 0 on the diamonds that disagree except T3 face 37 (6); the large counts sit on the faces beside the box, which the chord drawing cannot order at all (T2 81, T3 155, LL2 106-138 bends over 9-30 checked corridors).
Not covered: corridors with a wall without interior crossings (T2 9, T3 15, LL2 18 each) have no cross-wall order; bends skip the stretches with no usable segment range; copy strands (doubled lanes) were not separated, the base strand's segments were used.
