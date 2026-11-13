# G3.3b-2: the joint model in C++, a port of the prototype (DESIGN-G3 §G3.3, D235-D237)

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

## Goal
- Placement can record the relation system of its last X and Y rounds, with each constant's dependence on a bend's length, as a value. This is the uncommitted dump hook made permanent, without the JSON and the global.
- A new function builds the prototype's joint ILP from that record on `JunctionMilp` and solves it. This is `solve.py` ported to C++.
- Nothing calls it outside tests yet, so no layout changes.
- The oracle is the prototype's own results, on T and LL_RR x L_R.

## State
- Worktree `/home/pete/OpenTTD/.claude/nodes`. First: `git -C /home/pete/OpenTTD/.claude/nodes checkout -B wt-g33b2 <HEAD given at launch>`. Builds `build` (Debug) and `build-rel` are configured with HiGHS.
- Python with HiGHS, to re-run the prototype: `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/highs-venv/bin/python`.
- Output files in `/tmp/claude-1000/-home-pete-OpenTTD/021af430-3972-438e-9e4b-c282ea3899fd/scratchpad/g33b2/`.

## Read first (once each, offset/limit)
- `CODINGSTYLE.md`;
- `docs/junction-tool/DESIGN-G3-vectors.md` §G3.3 and §G3.3a;
- `docs/junction-tool/tools/g3proto/RESULT.md` "Model" and "Per topology";
- `docs/junction-tool/tools/g3proto/solve.py` (all, 360 lines): this is the model to port;
- `docs/junction-tool/wip/g3p-dump-hook.patch` (all): this is the tagging to make permanent. Every `G3Tag` site in it is a site you tag;
- `src/junction/placement/junction_milp.h`;
- `src/junction/placement/junction_placement.h` 30-120;
- `src/junction/placement/CLAUDE.md`.

## Types (in `junction_placement.h`, next to `JunctionPlacementChoices`)
Mirror the dump's JSON fields that `solve.py` reads, one member per field and with the same names, as plain structs:
```cpp
/** A relation x_v - x_u >= c of one axis of the placement's last rounds, with what c depends on (DESIGN-G3 §G3.3). */
struct JunctionSystemRelation {
	uint8_t axis; ///< 0 X, 1 Y.
	int v, u, c;
	int bend = -1; ///< When >= 0 and not nonlinear: c = a * L(bend) + b.
	int a = 0, b = 0;
	bool nonlinear = false; ///< c depends on lengths non-linearly; held at c.
	bool zero_branch = false; ///< Holds only when the bend's S-fan size is 0 (see solve.py), else only when it is not.
	std::string reason; ///< The why() text with edge, node and row.
};
```
- Add `JunctionSystemBend`, `JunctionSystemElement` (for the chains the cost reads) and `JunctionPlacementSystem`, holding the vectors and scalars the dump writes (`bends`, `chains`, `paths`, `pt_count`, `item_pt`, `port_row`, `port_offset`, `rows_known`, the prices it needs).
- Name each member after the JSON key. Write a one-line doc comment per member, taken from the hook's comment for that key.
- Add no member that `solve.py` does not read.

Then:
```cpp
/** The joint ILP's result (DESIGN-G3 §G3.3). */
struct JunctionJointResult {
	JunctionMilp::Status status;
	std::vector<int> length; ///< Per JunctionPlacementSystem bend, its L.
	double objective = 0;
	std::vector<std::string> conflict; ///< When infeasible: the reasons of the infeasible set's relation rows, in row order.
};
JunctionJointResult SolveJunctionJoint(const JunctionPlacementSystem &system, const JunctionSpec &spec);
```
- Put these in a new pair, `src/junction/placement/junction_joint.h/.cpp`, added to the placement CMakeLists. `JunctionJointResult` goes in junction_joint.h.
- `SolveJunctionJoint` builds exactly `solve.py`'s `build()`: variables, floors, the S-fan binaries, the relation rows with `a L + b`, the link rows, the cost rows and the objective with its epsilon. It solves with integrality on. Port the functions one by one and keep their comments.

## Recording
- `PlaceTrackGraph` gets one more trailing parameter: `JunctionPlacementSystem *system = nullptr`. When it is non-null, it is filled from the last X system and the last Y system of the call, as the hook did.
- Tag each relation at the hook's sites, with the hook's `a`, `b`, `nonlinear` and `zero_branch` values. Keep the tag as a member of the build's local state, not a global.
- Add a public wrapper in `junction_placement.h`, next to the other entries: `JunctionPlacementSystem RecordJunctionPlacementSystem(<the args of PlaceJunctionTrackGraph without error>, const JunctionPlacementChoices &choices);`. It places with those fixed choices and returns the record.
- Recording must not change any relation, solve or result. A call with `system == nullptr` must behave exactly as today.

## Tests (`src/tests/junction/placement/junction_joint.cpp`, tag `[junction][milp]`), outer first
1. **"joint: reproduces the prototype on T and LL_RR x L_R".**
   - For each, under pairing any at `MakeTestJunctionSpec()`: seed as today, record the system with the seed's choices, and solve the joint model.
   - Expect: status Optimal.
   - On T: every L equals the seed's m, except the joiner Corner of edge 5, token 0, which is 5 (today 6).
   - On LL_RR x L_R: every L equals the seed's m.
   - The objective equals the prototype's: T 27050, LL_RR x L_R 86350, within 0.5, after subtracting the epsilon term. If the epsilon term is not separable, compare the objective minus 0.001 × ΣL.
   - Red: write the test with `SolveJunctionJoint` returning `Status::Error`, record the failure, then port.
2. **"joint: recording does not change placement".** For T and LL_RR x L_R, placing with a non-null system gives a result equal to placing without one: `item_rows`, `ports`, `nodes`, `edges`, `size` and `choices`.
3. **"joint: an infeasible system names its conflict".** Record LL_RR x LL_RR at its default cut. If recording is not possible because placement fails before the last Y round, record the last system it reaches, as the hook did for failing topologies. Expect Infeasible, and that `conflict` holds the three reasons of RESULT.md's IIS chain (stack rows row 1, row nodes, S-bend size edge 45), in any order.
   - **The IIS may have changed since the prototype.** G3.1b changed row items (LL_RR x LL_RR node 24), so this chain may no longer hold. If it doesn't, the test records whatever is found: WARN the chain, CHECK only that the status is Infeasible or Optimal with a non-empty result, and report the chain verbatim. Do not stop for this.

## Cross-check with the prototype (not a gate, report only)
Do not re-dump. Run `solve.py` on the committed dumps in `docs/junction-tool/tools/g3proto/` and confirm its printed numbers still match RESULT.md. Report yes or no.

## Do not
- Call `SolveJunctionJoint` or `RecordJunctionPlacementSystem` from the pipeline, hover or seed.
- Change any relation, floor, rule or round.
- Add the curve-gap counters, hug inequalities or clash constraints of §G3.3a. Those are later steps.
- Commit any JSON writer or global.
- Use `pkill`, a broad `kill`, or a bare `git stash`.

## Stop conditions
- A hook site's tag cannot be placed on today's code because the code moved since the patch: name the site and what changed.
- Test 1 differs from the prototype in any L or the objective: report each difference, with the C++ and Python model sizes (variables, rows), and stop before changing the model to make it pass.
- Identity or any existing test changes.
- Anything not covered.

## Gates
- 0 new warnings;
- the new tests by name (Debug);
- `tools/junction/jt fast build '[topology]' 5`;
- `tools/junction/jt gate build-rel`;
- `tools/junction/jt identity build-rel`.
- The solve time of test 1 per topology, Release, reported.

CLAUDE.md, with no D-ids or design, brief or recon references, in `src/junction/placement/CLAUDE.md`: add "- Placement can record its last X and Y relation systems with each constant's dependence on a bend's length (`RecordJunctionPlacementSystem`); `SolveJunctionJoint` solves both axes and every bend length together as one integer program whose objective is the flat build cost."

One commit on wt-g33b2: `Add: Junction joint placement model on the MILP seam`, ending with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Report (short)
- references read;
- the commit;
- red evidence;
- per topology: model size, status, objective, the L that differ from the seed, and the time;
- test 3's chain;
- the cross-check;
- types and members added;
- gate and identity counts and times;
- stops;
- `git status --porcelain` and `git log -1`.
When you have reported, stop.
