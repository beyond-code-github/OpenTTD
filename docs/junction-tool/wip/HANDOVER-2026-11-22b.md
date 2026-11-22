# HANDOVER 2026-11-22 (evening): from "drawable" to a domain test suite

Read this first after a /clear or compact. Then read wip/CARD-domain-rules.md (the next step) and the last ~15 bullets of wip/CARD-E5-bundle-ends.md (the evidence). All work is merged to feature/junction-tool; ng1 (branch wt-e1) is clean and level with feature.

## 1. Where we are in one paragraph
The engineer re-prioritised (D427): get every hub working with today's compactness, and stop chasing V's (PSG 197's) compactness or geometry. Today we found that the drawing program's "clean" verdicts were wrong. Its keep-apart skip rules let real clashes through, and an independent clash check found overlapping tracks on T1, T2 and 197. Three stricter rules (all probes, off by default) fixed most leaks. T1 and T2 drew clash-free on the tile grid, and 197 did too, given 27 s. But each rule was tuned on one hub's picture, and fixes kept breaking other hubs (the crossing-flank rule went through three cuts). The engineer stopped this: the rules are our domain and should be tested offline against an independent truth. Next session builds that (wip/CARD-domain-rules.md, option A).

## 2. Decisions today
- **D427** (engineer): a working drawing for any hub comes first. Clarified: "I didn't say we give up on compactness, we just give up on trying to be *as compact* as V - just stick with whatever we have now and get it over the line for all hubs". Not a licence to loosen the model.
- **D428** (engineer, plan of record for next session): build the domain predicate and the rule test suite (wip/CARD-domain-rules.md, option A) before any further rule tuning on hubs.
- **Deferred** (engineer: "happy to defer"): widening Shape's S-bend (step) template to cover the 5 of V's 34 stepped stretches it cannot express. Revisit in Slots design, once keep-apart is between whole straight runs.
- **Not ruled, discussed:** the side choice by construction. Pairs whose side follows from headings, with a guaranteed margin of at least the spacing (Gemini's trap), get fixed sides. Pairs in convex faces keep their rows with fixed sides (not dropped). Other pairs get binaries. The pipeline's drawing only warm-starts the MIP. The engineer asked "why do we need a repair?"; this design needs none. Exact lazy separation (Gemini) would amend D381's wording; not raised for ruling yet.
- **Reframe (agreed in discussion):** the drawing program is the Slots v2 prototype, promoted step by step rather than rewritten. Its gaps to Slots: whole tiles, missing rule families (coverage, node legs to events, box-edge clearance), hard rows, the real objective, the speed budget (seed 5 s), and the hand-off output.
- **Rejected / dead:** convex cells as a fallback (worse on every hub, K1/K2); trying more Structure variants as the cure for T3/LL2 (0/8 each); the old composer ("No, we must not use the old stuff").

## 3. What is proven today (with commits and logs under docs/junction-tool/wip/probes/e5land/)
- **Keep-apart rows hold on V's real geometry wherever V's tracks are pinned** (vres-a54b3a903c.log, 2ac8d358ab). All 14 stretched rows involve the 10 unpinned tracks.
- **Why 10 of V's tracks do not pin** (script over tools/junction/corpus/out/psg197/steps.txt, lengths.txt, shape.txt):
  - S: the test's step matcher misreads staircases `a b a b` as two steps (L38, L44, L77). A test bug.
  - R: two same-side steps on one piece (L3, L72, L85). Flex C cannot express it.
  - O: right step before left (L8). Flex C fixes left first; 4 half tiles off.
  - F: a step between two crossings on one piece (L89). Flex is not placed in crossing runs.
  - X: zero-length lines (L99, L101). Extractor or pin data.
- **The pipeline with today's defaults** (top 8 variants, anyhub-k3all): T1 and T2 8/8 slack 0; 197 LP clean; T3 0/8; LL2 0/8. Convex cells: 0/33.
- **Whole tiles is not the blocker:** grid rows in the final draw T1 and T2 in under 1 s (anyhub-grid-0327295d0f.log).
- **197's integer speed problem was zero-length clutter:** `final_contract` removes held S-bend inserts and freezes committed ones as fixed steps. Pairs 14239 -> 1166; 197 on the grid Optimal in 194 ms (contract-*-a2385e6bda.log). Off the grid it still times out; grid rows help.
- **The "clean" drawings were not clean:** the dump-based clash check (`planar_dump.py clash`) found T1 1 overlap, T2 7 overlaps, 197 6. Every one traced to a skip rule: `zero contraction`, `span`, `adjacent` (clash-d0d1276432/).
- **Three strict rules** (options, off by default), final semantics:
  - `strict_span`: a span piece is exempt only against a crosser's own crossing piece.
  - `strict_zero`: different-track pairs the zero contraction would skip are kept; same-track pairs stay skipped (a track continuing through zero pieces). Walls crossing at the touch point are skipped. Each touching end is exempt against the other wall, and the far ends keep full spacing. Full spacing everywhere when both walls leave the touch point in the same direction, or are parallel with an interior touch. Directions are read in the track frame (`v.tracks[t].pieces[p]`), never the face walk's heading.
  - `crossing_flanks`: a structure's crosser whose crossing piece is the flank of an S-bend keeps the crossing 1 half tile off that S-bend's side. Current cut: only when the S-bend's insert is parallel (mod 4) to the partner's crossing piece.
- **Results with merged S-bends + grid + the strict rules:**

  | hub | broad flank rule (any S-bend) | parallel-only flank rule (current) |
  |---|---|---|
  | T1 | clean, 14 ms | clean, 18 ms |
  | T2 | **clean, 272 ms** | total slack 22, infeasible |
  | T3 | total slack 34; certificate face 38 Infeasible | total slack 110; **all certificates Optimal** (faces 29, 38, 75, 37, 66 together) |
  | LL2 | 28 | 20 |
  | 197 | LP slack 0; final 27 s at a 60 s cap (clean); fails the 15 s cap | not rerun |

  An eighth-of-the-insert version (a linear stand-in for "if used") was too weak: T2 total slack 14.
- **T2's specific geometry** (png/svg of faces 19 and 25): B2 crosses F1 at (48,58). Its S-bend's leading flank, which holds the crossing, can be 0, so the S-bend's north insert starts at the crossing and runs along F1 for one tile.
- **T3's knot** (faces 29 and 38, png): lanes B1, B2 and B3 turn from west to north-west while F3 (vertical) and D3 (horizontal) cross them. The lanes' turn points are in the wrong order, and B2's short committed steps (pieces 14-18) are crammed between B1 and B3. Sibling lanes (offset curves) made it worse there (total slack 34 -> 86).
- **LL2:** face 33 is still proven undrawable (copy F1" closing inside a face with facing bends): an upstream gap, needing the promise clause in Structure/Shape. Gemini's general form: no widening event (copy, prio row, fray step) inside a face pinched by opposing bends; the "spare lane" margin must be proven, not padded.

## 4. Tools built today (all off by default or offline)
- `PlanarDrawOptions::dump_path` and `PlanarDrawPipelineOptions::dump_dir`: JSON per run (tracks, faces, stretched rows, pairs with every candidate's margin, `skipped` with reasons, `kept`, `crossings`, `ends`). Caveat: the dump's margin formula ignores strict_zero's touch-end exemption, so it can show negative margins on exempt combos.
- `tools/junction/planar_dump.py`: `rows`, `pair`, `face`, `diff`, `svg`, `png` (no packages; the orchestrator can view PNGs directly, which it cannot do with SVGs), `clash` (overlaps and off-crossing touches traced to pairs and skip reasons; it does not check spacing).
- Env hooks in the any-hub test ("planar draw: the simplest drawing on every hub", `[.][planaranyhub]`): `PLANAR_ANYHUB_ONLY=<hub>`, `PLANAR_ANYHUB_K3_ONLY` (a T3 run in 10 s), `PLANAR_ANYHUB_CONTRACT`, `PLANAR_ANYHUB_GRID`, `PLANAR_ANYHUB_START`, `PLANAR_ANYHUB_FINAL_S`, `PLANAR_ANYHUB_FREE_<hub>=f1,f2`, `PLANAR_ANYHUB_SOFT_SEP`, `PLANAR_ANYHUB_CONFLICT`, `PLANAR_STRICT_SPAN`, `PLANAR_STRICT_ZERO`, `PLANAR_CROSSING_FLANKS`, `PLANAR_PROBE_SIBLINGS`, `PLANAR_PROBE_FACING`, `PLANAR_DUMP=<dir>`. The certificate test ("crowding certificates on T3 and LL2") reads the three strict env vars and has T3 cases 38, 37, {29,38} and {29,38,75,37,66}.
- Pipeline members: `final_contract`, `final_free_faces`, `final_pairs`, `contracted`, `contract_steps`, `final_variant`, `final_sides`.

## 5. Caveats and lessons
- HiGHS overruns its time cap (39-40 s on a 15 s cap in presolve). Keep an outer wall limit of about 5 minutes per run.
- A 60 s final for 197 broke the 15 s probe budget. Say so before doing that again.
- The agent briefs were in the main checkout's wip/, not in ng1. Agents read them there; that is fine.
- Lesson (engineer): diagnose with pictures and local tests, not hub runs. Stop at the second similar fix and write the general rule against an independent truth.

## 6. Next session
1. Wait for the engineer's go-ahead on wip/CARD-domain-rules.md (he said "we'll tackle this tomorrow").
2. Step 1: the domain predicate (tracks as points + crossings/structures/nodes -> legal or why), rasterising to tiles and applying `JunctionTracksMayShareTile`, with structure middles exempt only for their listed crossers. Hand cases from the game. New `[planardomain]` tests. One agent, test-first.
3. Step 2: the enumerator and the encoding report on two configurations (S-bend at a crossing; legs at a node) with today's rows, all three probes on.
4. Then the remaining configurations, then the rules fixed from the report one general rule at a time, then the hubs.
5. Still open after that: side choice by construction (T3); the LL2 clause; 197's final speed under the full rules; the missing rule families; the hand-off.

## 7. Gemini this session
- Prompts: wip/gemini/2026-11-22-architecture.md and wip/gemini/2026-11-22-route-to-working.md.
- Reply 1, digested: (C) now plus a broader LL2 clause; some binaries are irreducible; they belong in Slots as a MIP. Exact lazy separation counts as by construction. Formulation tips: pairs between maximal straight chains, per-pair big-M, 2-binary encoding (halves the count but is not stronger), indicators. Unverified citation details (Bast et al. ">95% never activate", the section number): verify before any PAPER note.
- Reply 2, digested: a MIP start is sound by construction. Two traps: convex faces keep their rows; a monotone walk guarantees margin 0, not the spacing. Contract unused flex before the integer final (this worked). Put the missing rules before tuning grid speed. Discounted: its detailed GF(2) parity recipe, and the "hidden blockers" (already covered by our tile rule, span limits, bay rule and Heights).
