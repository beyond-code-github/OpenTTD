# For Gemini: is this converging? Our honest read, and a proposed change of approach

## Step 1 done (your recommendation)
- An infeasible prune is now undone (inserts, drawing and its sides restored). Slack prices split: discovery vs from the commit on.
- Baseline at prices 1/1 (last LP round; slack = sum of broken-rule half tiles):

| Hub | Slack | What bends | Steps, box |
|---|---|---|---|
| T1 | 0 (legal) | — | — |
| T2 | 1 | one keep-apart row | 12, 64x77 |
| T3 | 15 | curve 6, keep-apart 9 (prune undone) | 17, 92x105 |
| LL2 | 16 | curve 3, keep-apart 13 (prune undone) | 29, 181x136 |
| PSG 197 | 1 | one curve row between two of Shape's fixed bends across a node | 2, 91x146 |

- Discovery 10 / commit-on 100: no hub better (T2 worse, 5). You agreed the 1-short rows are real conflicts.
- Decodes: 8 of LL2's keep-apart rows are two tracks (one east, one west) meeting near a point through zero-length pieces on both, likely their crossing point; one LL2 row has a track lying on top of another; T3's rows are three lanes of one route group a half tile apart where they need a lane.

## The question we are asking ourselves
We have been fixing one case at a time all day. Is it converging? Our read splits it in three:

1. **Keep-apart skip gaps: yes, they end, but we fixed them piecemeal.** Same track through zero pieces, then parts of one line across a node, then a track ending at a node and the next starting there, then any two walls reaching one node, and next would be two walls reaching one crossing. These are one rule: two walls need no keep-apart row when they touch once every may-be-zero piece is contracted to a point. In the planar map walls can only touch on one track, at a node, or at a crossing (the map's only vertices), so one rule covering all three closes the class with a stated guarantee instead of finding the next case on the next hub.
2. **The pipeline ending a unit short: no, patching does not converge.** Every correct rule change moves the discovery path, and the commit then lands a unit short on rows it has no move for (curve rows between Shape's fixed bends; a prune whose held inserts break hard rows). The prune guard and the prices only move where it gets stuck. This is D422's split (discovery picks inserts without their curve rows) and needs a design change: your Scheme A (ramp the curve penalty over discovery rounds, inserts stay free, the price of an insert's curve rows following its previous length as IRL1 does) or a rethink of D422.
3. **Real crowding (T3's lanes half a tile apart, LL2's track on top of another): unknown, and probably upstream.** Our solver is a stack of layers (Structure, Shape, then lengths); each promises the next that its output is drawable. If Shape hands the drawing program lanes that cannot be a lane apart, that is a Shape promise gap to check at Shape's output, not something to repair in the drawing program.

Proposed change of approach: (a) replace the piecemeal skips with the one contraction rule and its guarantee; (b) stop patching the pipeline; write the Scheme A design card for the engineer's decision; (c) decode the two crowding cases once as candidate upstream promise gaps.

## Questions
1. Do you agree with the three-way split and with stopping the pipeline patches? Anything we are missing?
2. Is the contraction rule sound as stated, and is "touch on one track, at a node or at a crossing" really complete for a planar map of tracks (what about two walls whose zero-length chains reach two different vertices that coincide in the drawing)?
3. For Scheme A, what makes an iterative reweighting like this converge (or cycle), and what stopping and tripwire rules would you write into the card?
4. Is there a smarter way to make the drawing stage robust to path dependence than redesigning discovery, for example solving the commit decisions jointly with a small number of binaries only where an insert is near the commit threshold?
