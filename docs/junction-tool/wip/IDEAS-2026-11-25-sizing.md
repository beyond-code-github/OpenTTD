# Ideas and findings, 2026-11-24/25: where SSS v2 stands, and how sizing could get fast

Read with: HANDOVER-2026-11-24-d456.md (UPDATE 10 onwards), RECON-v1-v2.md (the three approaches), DESIGN-d449-slots-chooser.md, ng2 `docs/junction-tool/wip/probes/d449/t2-d458.md` and `probes/legacy/{domain-legacy,generality,slots-chooser-t2}.md`.

## 1. The engineer's criteria (stated this session; memory any-hub-architecture)
1. **Any topology.** Whatever we build must work from the topology text alone, for every hub (LL3, LLL_RRR and beyond). The pre-SSS generator was retired for not reaching them.
2. **True compactness / expressiveness.** The pre-SSS generator "made rudimentary optimisation errors ... that proved almost impossible to close"; SSS exists "to actually facilitate true compactness".
3. **No dead end.** "What I don't want to do is end up with a dead end - something that can't be tweaked or improved."
4. **Fast.** Seed under 5 s (D297), hover under 100 ms; "60 seconds is far too slow".
5. **Learn from edits.** Manual compactions of a hub must feed back into the generator (they never could in the pre-SSS generator).

## 2. What landed (ng2 wt-c2, all gates green but known [!shouldfail])
- D458 mirrored crossings in structures, D459 corridor on by default (13ac73e2a7).
- D460 a crosser may run along under its deck: D438's along mode extended to bridges and to both sides of the crossing (af075e4600).
- The `span` row counts diagonal crossers' footprints and along runs (1ad12c9973): a pre-existing looseness.
- The domain: a bridge head's deck-side end is no ground end (game pin 5b, e5d960278e; fix 01f112979f).
- T2 drawn judge- and domain-legal from two P6 CP-SAT witnesses (pinned t2-p6cf, 7fc05c5614); built in the rig: legal but stretched (68x50; W and E ports 35 tiles apart: nothing ties opposite ports, and the witness came from a feasibility-only solve).
- D461: CP-SAT's licence is not a constraint while the architecture is open (a solver server later if needed).
- Probe knobs: PLANAR_STAGEB_{NOFOOTPRINT,UNPIN,ELASTIC,PAIRFILTER,NOSTRICTSPAN}, PLANAR_STAGEA_{CROSSFLANKS,PAIR_INFO}, PLANAR_SLOTS_CHOOSER; `pairfilter_json.py` (CP-SAT deletion filter on an export).

## 3. Measured this session
- **T3 is refuted at stage A** (one pair, F3 vs F2 sibling separators; one tile of span short on structure 9: span + 1 tile makes it feasible). An upstream promise gap: Structure/Shape check each span alone.
- **The older approaches under today's domain** (physical-piece adapter, `[planardomainlegacy]`): B's T1, T2, T3, hand LL2 and A's LL_RR x L_R are all domain-legal, 0 breaks; B's old checker 0 violations (mapped LL2: 1 signal violation).
- **Generality matrix per layer** (`[generality]`, T1-T5, LL2, PSG 197):
  - Structure: A passes all (< 5 ms); B passes T1-T5, LL2, fails 197 sites; C passes T1-T3, LL2, fails T4/T5 (node-group crossing rule) and plain 197 (64-bit variant mask); pinned 197 loads.
  - Shape: A and B pass T1-T5, LL2; B fails 197; C passes T1-T3, LL2; pinned 197 shape breaks H12 (57; H12 is the retired cell check, probably stale).
  - Sizing: A legal on T1 and LL_RR x L_R only (T2/LL2 contradictory rules, T3 timeout, T4/T5 ~31 GB OOM: keep these hidden-only); B legal on T1-T3, LL2 (LL2 122 s), fails T4/T5 ("mixed site not composed yet"); C: T1, and T2 only by offline CP-SAT.
  - B's slot layer: T1-T5 and LL2 in 3-6 ms; fails 197 site resolution.
- **Slots chooser on T2 (hard form): NO.** C's variant matching B's o1t3f1s0 is rank 1760 in C's ranking; with B's lane offsets, stage A hits its 15 s cap, stage B is Infeasible at its listing solve on C's own rows (cross, along, deck, footprint), not the slot rows: the stage A / stage B split, not B's plan.
- **Where the time goes:** with sides and S-bends fixed, C's sizing is an LP-like solve of 15-700 ms (stage B rounds; T2 LP with all pairs slack 0; 197 drawing LP slack 0). All slowness is in the discrete choices (keep-apart sides, S-bends, grid parity).
- **CP-SAT vs HiGHS:** CP-SAT refutes or certifies local questions (a pair, a span, a face) in about a second; finding a whole-hub drawing is slow (T2 lazy loop 22-26 rounds of 10-36 s; all pairs UNKNOWN at 120 s), but only ever on the big-M MILP export.

## 4. Ideas and how they stand
1. **The three approaches (RECON-v1-v2.md).** A and B were fast because sideways decisions came from something small before geometry (A: templates; B: slot layer, bundles); both stop at sizing because those decisions are built into the representation (the dead end). C's representation is exact and can carry an expert hub (V's 197 at V's points): keep it; borrow only as steering that never removes a legal drawing (the expressiveness guard in DESIGN-d449-slots-chooser.md).
2. **"No dead end" as a principle (proposed ruling, not yet recorded):** the representation stays exact and expressive (D456: feasible set = domain-legal drawings); heuristics (chooser, warm starts, slot offsets, objective, solver) are pluggable steering that never restricts; every rule domain-checked; progress measured (generality matrix, answer keys). Awaiting the engineer's OK to record.
3. **Edits as answer keys (the learning loop).** Rig edit -> extract to a drawing -> domain verdict -> load at each layer and diff against the generator -> the first differing layer names the component to fix (Structure ranking, Shape objective, chooser, length objective) -> the edit stays as an answer-key test. Needs a working generator first (engineer: "we haven't managed to build a version of that which actually works").
4. **Promises must be sufficient, not just necessary.** T3 and LL2 face 33 keep every Structure/Shape promise yet have no drawing. Make the promises local and checkable (span capacity across sibling separators; per-face drawability; the pair conflicts T2 showed), each decided in about a second, run over every handed-down variant; every failure becomes a promise clause (red test first). Engineer: "the oracle is too slow to be useful" for whole hubs; local questions are fast.
5. **The fast solve = TSM's metrics step.** Once sides and S-bends are decided with a drawability guarantee, sizing is one LP plus grid rounding (tens of ms). The open problem since the week began: deciding them by rule with a guarantee, without templates (walk_strict/facing decide 9-64% of pairs on fixtures, ~31% on 197; cells retired D431). Engineer: "Isn't this what we've been trying to do all week".
6. **Outside view (offered, not run):** a literature scan (metro-map / octilinear / VLSI compaction: how fixed-topology layouts get fast and compact) and an independent design review of the sizing layer.
7. **Corpus (engineer's suggestion; Gemini: TSM/SSS is viable).** 252 PSG saves pulled (178 load), thousands of hubs; bottleneck is the extractor (one hub, 197, done). Plan: repeatable extractor on five hubs (197, 186 Brainmelter, 227 mfb, 248 Furball, 188 Bridge split); each hub: domain at its own points, Structure/Shape load, promise failures; mine side and S-bend decisions against local features (197 already: step toward the sibling after a split 35/36, away before a merge 29/40); answer keys for compactness; ranking data (which variant the expert chose).
8. **S-bends decided with lengths (the engineer's unlock).** "I just don't see how we can know what bends we need without an awareness of the lengths." Bends and lengths must be one decision. Flex inserts already let length decide a bend (0 = none, D383); it became combinatorial because rules depend on whether a bend exists, and big-M MILP is bad at that, so we split into stages and choosers that decide bends without lengths. Model it directly instead: a joint CP model where "this bend exists" is a literal that switches its rules (least length, turn, curve, flank, keep-apart) on or off.
9. **No offered positions, only a rule-derived capacity.** Engineer: "isn't the whole concept of 'offering bends' flawed? Surely we just want the solver to decide how many bends are needed and where." An offer done right is capacity, not location: a step between free-length flanks can sit anywhere on the stretch and vanish at length 0; alternating offers allow any order (D442). What does restrict today: flex C only on one-piece stretches and a stretch's first and last pieces, and not in lattices: positional assumptions that may block compact answers. The cap is not a choice: the curve rules (opposite-hand curves 2 half tiles apart; windows min(2 TL - 1, n CL), 7 half tiles with TL 4) bound how many bends fit a stretch of length L (engineer: "a piece of track 7 tiles long can have at most two bends in it and still maintain curve length"); so give every piece of every stretch (except inside spans) as many step slots as fit at the box's largest length, and let the curve rows, conditional on existence, decide which are usable at the length chosen.
10. **CSP over MILP (engineer: "should we be really leaning in to this as a constraint satisfaction problem?").** Why we had HiGHS: the geometry began linear (difference constraints, then a MILP with an area objective), LP diagnostics were useful, HiGHS is MIT (CP-SAT Apache-2.0: offline only until D461; Chuffed and Gecode lost the D450 benchmark). The problem became combinatorial (sides, bends, conditional rules), where CP is strong and big-M MILP weak. Native form: enforcement literals (`OnlyEnforceIf`) instead of big-M, no stage split (stage A/B exists only because HiGHS could not take every exact row at once; today's slots probe failed exactly at that split).

## 5. Proposed next experiment (not started; engineer to confirm)
A native CP-SAT model of T2 (design first, in the orchestrator): every exact rule of the drawing program, keep-apart sides and S-bend existence as literals, rule-derived step slots on every piece outside spans, one solve, no stages, no chooser; optional B slot offsets as hints. Measure time (vs 5 s), domain verdict, box and length vs B's 94 x 29; raise the slot cap to check it is not binding. Then T3, LL2; then the local promise checks over every variant with the same model; corpus extraction in parallel as light work.

## 6. Open for the engineer
- Record the "no dead end" ruling (idea 2)?
- The joint CP experiment (section 5) vs an outside view first (idea 6) vs corpus first (idea 7)?
- C's Structure fixes for T4/T5 and plain 197 (local, outside-in) can run alongside any of them.

## 7. Process notes
- Agents slipped to sed/heredocs/Python/`git checkout` several times despite "Edit tool only"; one launched `build-rel/openttd -t` by mistake (exited at once). Results unaffected.
- A's placement on T4/T5 reached ~31 GB before the OOM killer: never in a gate.
- The rig server on 127.0.0.1:3989 (T2 legal drawing in company #1) is stopped at parking.
