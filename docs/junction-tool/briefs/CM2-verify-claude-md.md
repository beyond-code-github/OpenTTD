# CM2: verify and correct the CLAUDE.md draft line by line

Edit only /home/pete/OpenTTD/docs/junction-tool/CLAUDE-draft.md. Read-only otherwise. Keep your context small.

## State
- CM1 (brief docs/junction-tool/briefs/CM1-claude-md.md: read its Format section) wrote the draft (263 lines) from DECISIONS.md, DESIGN-G1, TESTS.md, STATUS and some briefs. Its own doubts: compiled status of old pipeline files; the signal pass owner; the near-search limits; QL2; the hover cache (D93); prio coverage rules (D108/D148/D148a) in the graph pipeline; TU5 stated as current; spent vs quoted wording; D51 shift-click. Known corrections from the orchestrator: TU5 (tunnel flip) is NOT merged (parked WIP on branch wt-tu5): the draft must not state it; tunnels are built by TU1-TU4 only when a test sets a crossing to tunnel; QL2 (train/prio persist in the client config across sessions) IS merged; CN1 (hover search cancels on cursor move; spent vs quoted as an information message after a build) IS merged; P6b (level descent stays inside the cone's ranges) IS merged; LP1 (D221), TP1 (D222), CX1 is NOT merged (C1-C3 open; D223 is ruled but not implemented beyond the one-joiner rule); OF1, OF2, MJ1 (D224) merged; RW1-RW6b merged; TS1 (optional S-bends) NOT merged.
- Merged history: `git -C /home/pete/OpenTTD log --oneline -120` (commit subjects say what each step changed).

## Do
1. For EVERY fact line in the draft: open the cited source (DECISIONS row, DESIGN-G1 section, brief, or the code: grep src/junction_*.cpp / src/tests/junction_*.cpp) and confirm the line states the CURRENT behaviour. Fix the line in place, or delete it if it is wrong and cannot be fixed, or mark it in Open if it is undecided. Where the code and a decision disagree, the code's current behaviour wins if a later merged commit explains it; otherwise list it.
2. Resolve CM1's doubts from the code and git log (e.g. CMakeLists for what is compiled; who calls the old signal pass).
3. Keep the format rules (present tense, no dates/quotes/why, ids in brackets, 250-350 lines).

## Report
Counts: lines checked, fixed, deleted, moved to Open; a list of the material changes (one line each); remaining contradictions the engineer should rule on. When you have reported, stop.
