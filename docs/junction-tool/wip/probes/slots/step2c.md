
### STEP2C T1

- STEP2C T1 stage A (no keep-apart rows, tidy on): Optimal 169 ms

Per-track breaks:

- track CD1 (idx 4): i 3[middle h1 2] j 5[flank h-1 1]; zero run: 4[h0 flank]; crossing on i..j (event_piece): no; class C (1 flank(s) in the run, none or not between two middles)
- track AF1 (idx 5): i 2[flank h5 1] j 4[middle h3 1]; zero run: 3[h4 flank]; crossing on i..j (event_piece): no; class C (1 flank(s) in the run, none or not between two middles)
- track F1 (idx 11): i 5[middle h2 2] j 13[flank h4 4]; zero run: 6[h3 flank] 7[h4 middle] 8[h3 flank] 9[h4 flank] 10[h3 middle] 11[h4 flank] 12[h5 middle]; crossing on i..j (event_piece): yes; class C (4 flank(s) in the run, none or not between two middles)

Node breaks (ends read through): node turns FAIL (4 failing line(s))

- node 1 point (8,13) rule F6 trunk CD1 arriving h7 legs: D1 starting h1
  - leg D1 starting h1: first non-zero 3[middle h1 1]; zeros to node: 0[h0 flank] 1[h-1 middle] 2[h0 flank]; Shape end piece 0 h0; class N1
- node 3 point (11,14) rule F8 trunk AF1 starting h5 legs: A1 arriving h4 ; F1 arriving h4
  - leg A1 arriving h4: first non-zero 1[middle h4 2]; zeros to node: 4[h5 flank] 3[h6 middle] 2[h5 flank]; Shape end piece 4 h5; class N1
  - leg F1 arriving h4: first non-zero 17[flank h4 2]; zeros to node: none; Shape end piece 17 h4; class N3 (non-zero end piece is a flank)
- node 4 point (2,9) rule F8 trunk CB1 starting h6 legs: C1 arriving h6 ; B1 arriving h6
  - leg C1 arriving h6: first non-zero 1[middle h-2 2]; zeros to node: 4[h-1 flank] 3[h0 middle] 2[h-1 flank]; Shape end piece 4 h-1; class N1
  - leg B1 arriving h6: first non-zero 8[flank h6 2]; zeros to node: 12[h6 flank] 11[h7 middle] 10[h6 flank] 9[h5 middle]; Shape end piece 12 h6; class N3 (non-zero end piece is a flank)
- node 5 point (12,3) rule F8 trunk ED1 starting h2 legs: D1 arriving h2 ; E1 arriving h2
  - leg D1 arriving h2: first non-zero 12[middle h2 2]; zeros to node: 13[h1 flank]; Shape end piece 13 h1; class N1
  - leg E1 arriving h2: first non-zero 4[flank h2 2]; zeros to node: none; Shape end piece 4 h2; class N3 (non-zero end piece is a flank)

Counts STEP2C T1: per-track C=3; node [N1]=4 [N3]=3 [rule F6]=1 [rule F8]=3

### STEP2C T2

- STEP2C T2 stage A (no keep-apart rows, tidy on): Optimal 2360 ms

Per-track breaks:

- track A2 (idx 13): i 7[middle h7 1] j 9[flank h5 1]; zero run: 8[h6 flank]; crossing on i..j (event_piece): no; class C (1 flank(s) in the run, none or not between two middles)
- track D1 (idx 18): i 1[middle h0 2] j 9[flank h2 20]; zero run: 2[h1 flank] 3[h2 middle] 4[h1 flank] 5[h2 flank] 6[h1 middle] 7[h2 flank] 8[h3 middle]; crossing on i..j (event_piece): yes; class C (4 flank(s) in the run, none or not between two middles)
- track D2 (idx 19): i 1[middle h0 2] j 9[flank h2 12]; zero run: 2[h1 flank] 3[h2 middle] 4[h1 flank] 5[h2 flank] 6[h1 middle] 7[h2 flank] 8[h3 middle]; crossing on i..j (event_piece): yes; class C (4 flank(s) in the run, none or not between two middles)
- track F1 (idx 22): i 1[middle h2 2] j 9[flank h4 12]; zero run: 2[h3 flank] 3[h4 middle] 4[h3 flank] 5[h4 flank] 6[h3 middle] 7[h4 flank] 8[h5 middle]; crossing on i..j (event_piece): yes; class C (4 flank(s) in the run, none or not between two middles)

Node breaks (ends read through): node turns FAIL (7 failing line(s))

- node 0 point (15,22) rule F6 trunk AB1 arriving h7 legs: A1 starting h5
  - leg A1 starting h5: first non-zero 2[flank h5 1]; zeros to node: 0[h5 flank] 1[h4 middle]; Shape end piece 0 h5; class N3 (non-zero end piece is a flank)
- node 1 point (18,21) rule F6 trunk AB2 arriving h7 legs: A2 starting h5
  - leg A2 starting h5: first non-zero 1[middle h5 1]; zeros to node: 0[h6 flank]; Shape end piece 0 h6; class N1
- node 7 point (7,22) rule F8 trunk AF2 starting h4 legs: A2 arriving h5 ; F2 arriving h5
  - leg A2 arriving h5: first non-zero 9[flank h5 1]; zeros to node: 13[h5 flank] 12[h6 middle] 11[h5 flank] 10[h4 middle]; Shape end piece 13 h5; class N3 (non-zero end piece is a flank)
  - leg F2 arriving h5: first non-zero 11[middle h5 1]; zeros to node: 12[h4 flank]; Shape end piece 12 h4; class N1
- node 9 point (5,8) rule F8 trunk CB2 starting h7 legs: C2 arriving h7 ; B2 arriving h7
  - leg C2 arriving h7: first non-zero 9[middle h-1 1]; zeros to node: 12[h0 flank] 11[h1 middle] 10[h0 flank]; Shape end piece 12 h0; class N1
  - leg B2 arriving h7: first non-zero 8[flank h7 2]; zeros to node: 12[h7 flank] 11[h8 middle] 10[h7 flank] 9[h6 middle]; Shape end piece 12 h7; class N3 (non-zero end piece is a flank)
- node 11 point (21,22) rule F8 trunk ED2 starting h3 legs: D2 arriving h3 ; E2 arriving h3
  - leg D2 arriving h3: first non-zero 12[middle h3 3]; zeros to node: 13[h2 flank]; Shape end piece 13 h2; class N1
  - leg E2 arriving h3: first non-zero 8[flank h3 6]; zeros to node: none; Shape end piece 8 h3; class N3 (non-zero end piece is a flank)
- node 13 point (7,14) rule F6 trunk B1 arriving h6 legs: prio:B1 starting h0
  - leg prio:B1 starting h0: first non-zero 0[fixed h7 2]; zeros to node: none; Shape end piece 0 h7; class N2
- node 14 point (20,17) rule F6 trunk E1 arriving h3 legs: prio:E1 starting h1
  - leg prio:E1 starting h1: first non-zero 0[fixed h2 2]; zeros to node: none; Shape end piece 0 h2; class N2

Counts STEP2C T2: per-track C=4; node [N1]=4 [N2]=2 [N3]=4 [rule F6]=4 [rule F8]=3

## Reading

- Per-track breaks: T1 3 (class A 0, B 0, C 3), T2 4 (A 0, B 0, C 4); the same tracks as step 2b (T1 4, 5, 11; T2 13, 18, 19, 22). No run holds a fixed piece; no run has exactly one flank between two middles.
- Class C is two shapes: a short run of one flank next to a middle and a flank (T1 tracks 4 and 5, T2 track 13), and a long run of 7 zero pieces with 4 flanks and 3 middles between a nonzero middle and a nonzero flank, over a crossing (T1 track 11, T2 tracks 18, 19, 22).
- Node breaks: T1 4 nodes (F6 1, F8 3), 7 legs: N1 4, N2 0, N3 3. T2 7 nodes (F6 4, F8 3), 10 legs: N1 4, N2 2, N3 4.
- Every N3 leg ends in a non-zero flank (zero pieces between it and the node are flanks and middles); every N1 leg ends in a non-zero middle; both N2 legs are the zero-gap prio legs at T2 nodes 13 and 14 (fixed piece, no zero run).
