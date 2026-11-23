# BRIEF contraction-remap: a contracted crossing sits on the pieces at its own point

You execute this brief and nothing else. Make exactly the changes listed. Do not add any type, member, file, abstraction or mechanism that the brief does not name, and do not widen or narrow its scope. If something is not covered, or you would have to choose between approaches, stop at a clean state and report the question. A workaround that avoids a stop condition is a breach, not a stop. Keep your context small: send command output to files and report only what failed.

Budget: 45 minutes. No solves.

## 1. State
- Worktree `/home/pete/OpenTTD/.claude/ng2`, branch `wt-c2`, head `<HEAD>`. Build: Release `build-rel`. Load limit `JT_JOBS=4`, cmake `-j4`. Never touch ng1 (the rig runs its binary).
- Diagnosis: `docs/junction-tool/wip/probes/slots/contraction-diag.md` (T2 section). Rule broken: "a contracted event sits on the pieces carrying the crossing at its original point". `ContractedVariant`'s event remap (`src/tests/junction/planar/planar_domain_encoding.cpp` ~:1100-1115) takes the first pair of same-heading pieces that touch anywhere; on T2 crossing 5 (F1 x B1, true point (18,19)) lands on F1 piece 5 because B1 piece 3 touches it at (22,31).

## 2. Read first
- The diagnosis file (T2 section); `ContractedVariant` and `SegmentsMeetInOnePoint` in planar_domain_encoding.cpp; the pin test and the hidden `contraction diag` test.

## 3. Tests
- Split the pin test into two TEST_CASEs, T1 and T2 (same body per hub, shared by a file-local function): "... (T1)" stays `[junction][contraction][!shouldfail]` (T1's grid cause is a later step); "... (T2)" tagged `[junction][contraction]`.
- Red: the T2 case fails today with stage B "unsupported: structure 4's crossings sit on different pieces of its separator" (record it).
- After the fix the T2 case: stage B's program builds; then either every row holds (green) or it fails with broken tags. If broken tags remain, STOP after the fix commit: mark the T2 case `[!shouldfail]`, list every broken tag with the diagnosis-style cause (stage A rule too loose like T1's `half`, or something else), do not fix them.

## 4. Change
- The remap: for each crossing, the contracted pieces chosen are those of the two tracks that contain the crossing's ORIGINAL point (the point in stage A's uncontracted drawing, translated as the contraction translates); same-heading-and-touching remains a check, not the selector. If no contracted piece pair contains the point, `ContractedVariant` returns nothing (as it does for other refusals) and the test says so.
- `[planardraw]` budget: find its slowest test case (Release per-case times from `jt`'s log) and tag it `[slow]` if `[planardraw]` in Debug is over 5 s; report the case and times. If you cannot run Debug (no ng2 Debug build), measure in Release, report, and do not retag.

## 5. Do not
- No change to stage A, stage B rows, options or defaults. No knob. Source edits with the Edit or Write tool only (no sed, no Python).

## 6. Gates and commit
- `JT_JOBS=4 tools/junction/jt fast build-rel '[contraction]' 5`, `JT_JOBS=4 tools/junction/jt fast build-rel '[planardomain]'` (the contraction is used by the domain suite: every result unchanged; a changed result is a stop).
- Commit: `Fix: Junction: a contracted crossing sits on the pieces at its own point`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## 7. Report
References read; commit; red text; T2 after the fix (green, or the broken tags with causes); `[planardomain]` before/after counts; the `[planardraw]` timing outcome; stops; `git status --porcelain`, `git log -1`. When you have reported, stop.
