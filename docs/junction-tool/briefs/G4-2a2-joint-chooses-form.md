# G4 2a2: the joint chooses each crossing group's form (D260, D261, D266; DESIGN-G4 §11.2, §11.6)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
After 2a1b, each single-bridge band crossing carries both forms and placement places the one `choices.form` gives (default S-bend). Now the joint model (`SolveJunctionJoint`) chooses: one binary q per crossing group (1 square), the recording carries both forms' rows tagged with (group, form), and the joint guards each tagged row by q. Success: LL_RR x LL_RR round 0 at all floors, infeasible today in the S-bend form, becomes Optimal with q free.

## State
- Worktree `/home/pete/OpenTTD/.claude/ng1`: `git -C /home/pete/OpenTTD/.claude/ng1 checkout -B wt-2a2 <HEAD given at launch>`. `build` (Debug) and `build-rel` (Release) exist; delete stale `.pch` files if the build complains. Leave untracked files alone.
- Output files go in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/2a2/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md` (C++ part) — read it;
- `docs/junction-tool/DESIGN-G4-static.md` §11.1-§11.3 and §11.6 2a2 (lines 315-360, 405-416); `docs/junction-tool/DECISIONS.md` D260, D261, D266 (last rows);
- `src/junction/placement/junction_placement.h` lines 37-200 (choices, `JunctionSystemRelation`, `JunctionSystemBend`, `JunctionPlacementSystem`);
- `src/junction/placement/junction_placement.cpp`: `Elem` and `absent` (~215-330: `cross_groups`, `cross_form`, the absent branch), `SystemTag` / `ge` / `record_only_now` (~350-395), the relation record (~525-545), the Corner and S-bend size rows (~1760-1830), the chain runs and floors (~1850-1890), the square family "bridge over run" and the over-S-bend family with its two alternatives (~1890-1985: the pattern you copy), `sym_routes` and the Cross groups there;
- `src/junction/placement/junction_joint.cpp` lines 90-300 (counters with `zero`, bend sources, S fan Z, over rows with w and `alt`) and ~460-480 (choices read back);
- tests: `src/tests/junction/placement/junction_joint.cpp` ~1380-1430 (`ChoicesWithForms`, the LL² 100100000 test), ~40-100 (the seeds' objective tests);
- `src/junction/placement/CLAUDE.md`.

## Changes
1. **Points for form-dependent bends.** In the chain build, a Cross S-bend and a group's outer Corners (the group's first Corner `head + 1` and its last token `groups.last`) always get their own points (`new_pt()`), whatever the form. Inner Corners of a group (present in neither form) stay as 2a1b made them (absent, aliased). At the recording's form, the unchosen bend is held at size 0 by its own size rows (below); the layout, objectives and level hashes at a fixed form stay exactly as today.
2. **Tags.** `SystemTag` and `JunctionSystemRelation` gain `int form_group = -1; int form = -1; ///< When >= 0: the relation holds only when that crossing group's form is form (DESIGN-G4 §11.2).` `JunctionPlacementSystem` gains `std::vector<JunctionSystemFormGroup> form_groups;` with, next to `JunctionSystemOver`:
   `struct JunctionSystemFormGroup { int edge; int tok; int form; }; ///< A crossing group (DESIGN-G4 §11.1): its edge, its first token (the first quadruple's S-bend), and its form at the recording (0 S-bend, 1 square).`
   `JunctionSystemBend` gains `int form_group = -1; ///< For a crossing's S-bend or outer Corner, its group: the bend has its size only in its group's form (the S-bend in 0, the Corners in 1).` Every relation, bend and group is recorded in the order the build makes them; the record copies the new fields where it copies `alt`.
3. **Both families recorded, the unchosen one record-only** (the over-S-bend pattern, `record_only_now`): per crossing, the square family rows ("bridge over run") are tagged (group, 1) and the over-S-bend rows (t, both alternatives) tagged (group, 0); the rows of the form not at the recording are `record_only_now = true`, so the fixed path never solves them. Likewise per crossing group:
   - the S-bend's size rows tagged (group, 0) with its usual tag (record-only in the square form);
   - the outer Corners' "Corner size" rows tagged linear in L, `SystemTag{&el, dx / k, 0, ...}` and `{&el, dy / k, 0, ...}` with tags (group, 1) (record-only in the S-bend form), so c = a L with L = k in the square form;
   - in the S-bend form the zero-size rows that hold the outer Corners and the E/W run between them at 0 (x and y equalities to the element before), tagged (group, 0); in the square form the zero-size rows holding the S-bend at 0 tagged (group, 1); each set record-only when not at the recording's form;
   - the chain runs and floors around the group: a floor whose two ends are both present in only one form is tagged with that form; a floor that holds in both forms is untagged.
   The "no crossed sbend" refusal goes (every crossing carries its S-bend now).
4. **Bend sources.** A Cross S-bend keeps its source with `form_group` set; an outer Corner gets source `"cross_corner"`. Both are recorded in the S-bend form and the square form alike.
5. **The joint** (`junction_joint.cpp`): per form group one binary `q` (`model.Var(0, 1)`), and:
   - a Cross S-bend: its `zero` variable is a new `z` with row `z + q = 1` (it has its size only when q = 0), with the S fan rows' pattern `L >= floor z`, `L <= BIG z`;
   - a `"cross_corner"`: `L` variable with row `L - k q = 0`, and `zero[id] = q` (the counters relax its rows when it vanishes);
   - each relation with `form_group >= 0` is relaxed as `alt` is today: form 0 holds when q = 0 (`+ BIG q`), form 1 when q = 1 (`- BIG q`, `lower - BIG`);
   - curve gaps: today's counters with the `zero` relaxation; no combination rows (D266);
   - an optional parameter `const std::vector<int> *forms = nullptr` (per form group: -1 free, else fixes q to it) on `SolveJunctionJoint`;
   - the result's `choices.form` (at each group's `tok`: q) and `choices.run` (the run length read off the solution: `|x(last Corner.first) - x(first Corner.last)|` in the square form, 0 otherwise). The loop takes them as the next round's form and run (the joint now chooses them, so `round_choices.form` is no longer copied through).
6. **CLAUDE.md** (placement; standalone facts, no D-ids): the joint model chooses each crossing group's form by one binary; the recording carries both forms' rows, each holding only in its form, and the fixed path solves only the given form's rows; a crossing's S-bend and its outer Corners have their own points in both forms. Remove the line "the form and the run length are choices" only where it says the joint does not choose them.

## Tests (red first; record each red message)
In `src/tests/junction/placement/junction_joint.cpp`:
1. **"joint: at every form with q fixed, the joint equals that form's recording"** `[junction][milp]`: L_R x LL_RR (3 groups) at its seed's choices: for each of the 8 forms, record at that form (fixed path, 2a1b) and solve its joint (forms all fixed to it); also solve the superset recording (at the S-bend form) with `forms` fixed to it. Same status, and when Optimal the same objective within 0.05. Red: the parameter and groups do not exist (fails to compile is not red: add the members and an empty `form_groups` first; then red is the objective/status mismatch or an empty group list). If a form mismatches only because a curve-gap row is cut, STOP and report the form, the pair and both objectives (D266).
2. **"joint: LL² round 0 with q free is Optimal at no more than 268600"** `[junction][milp][.sweep]`: LL_RR x LL_RR (pairing any) at `JunctionFloorChoices` (S-bend form, its own port offset): record, solve with q free: Optimal, objective (flat cost, as the 100100000 test measures it) at most 268600.05; WARN the chosen form string (groups in `form_groups` order) and the objective. Red: today the S-bend form's joint is infeasible.
3. **"joint: T and LL_RR x L_R keep the S-bend form"** `[junction][milp]`: at their seeds' recordings with q free: Optimal at 27300 and 86350 (as the existing seed tests), every q 0.
Keep the existing LL² 100100000 test as it is.

## Re-pins
Recording sizes and system digests grow (points for form-dependent bends, record-only rows): re-pin those only, each listed (old -> new) in the report and commit body. Layouts, level hashes, identity numbers and the seeds' objectives: none. Any other changed number is a STOP. List L_R x LL_RR's joint objective at its seed if it moves (not pinned; its round 0 preferred e1 square).

## Do not
- Add the M helper, curve-gap combination rows, O as a variable, or any static occupancy (2b2a-2b3).
- Retire t, w, `choices.over`, `choices.over_after`, `JunctionSystemOver`, the over rows or `JunctionSBendColumnForm`.
- Touch the hover, the pipeline, topology or the arrangement.
- Touch `.claude/nodes2` or any worktree but ng1; use `pkill`, any broad `kill`, or any form of `git stash`. Stop a process of your own only by its PID.

## Stop conditions
- Any layout, objective, level hash or identity number moves; a seed stops seeding or its `CheckJunctionRules` changes.
- Test 1's curve-gap mismatch (above), or any other mismatch at a fixed form.
- Test 2 Optimal above 268600.05, or not Optimal: report status, the conflict and the time; do not tune.
- A crossing whose groups or elements are not as described.

## Gates (D264; the runner is the main checkout's, parallel)
- while working: `/home/pete/OpenTTD/tools/junction/jt fast /home/pete/OpenTTD/.claude/ng1/build '[milp]' 10` and `... '[topology]' 5`;
- before the commit: `/home/pete/OpenTTD/tools/junction/jt gate /home/pete/OpenTTD/.claude/ng1/build-rel`, `... sweep ...`, `... identity ...`: all pass within budget (30 s, 60 s, 45 s).
Give every `jt` call a Bash timeout of at least 400000 ms.

One commit on wt-2a2: `Feature: Junction joint chooses each crossing group's form`, body listing the re-pins, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read; the commit; red evidence per test; types, members and sources added;
- re-pins (old -> new); test 2's status, objective, form string and solve time; L_R x LL_RR's objective;
- gate, sweep and identity counts and times; stops; `git status --porcelain` and `git log -1`.

When you have reported, stop.
