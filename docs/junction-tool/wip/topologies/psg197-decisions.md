# PSG 197 decisions (Structure sidecar to psg197.txt)

Source: tunnels2.txt (50 tunnels, head to head), the save piece graph through the trace scripts; evidence rule: a line crosses a tunnel when a track piece lies on a tunnel middle tile. Crossings read from tiles, not from the notation. Frame, lane and route names as in psg197.txt.

Decision columns: separator = the line in the tunnel (tunnel under the crosser, kind tunnel); length = middle tiles = head-to-head distance - 1 (the model's tunnel length); group = parallel tunnels side by side with the same direction and length (adjacent entrance tiles); copy = a crossing line doubled over the group.

## Count summary

- Crossing structures: 50 tunnels, 0 bridges, 0 at-grade crossings in the hub (the 3 bridge tiles lie outside it).
- Tunnel groups: 29 (sizes: 1 x11, 2 x15, 3 x3).
- Middle tiles, tunnels by length: 1: 5, 2: 11, 3: 14, 4: 4, 5: 4, 6: 5, 7: 1, 12: 3, 13: 3.
- Separator x crosser line pairs from tile evidence: 112; of them 10 are also a crossing the notation derives from psg197.txt (47 derived in all); 102 are not.
- Sidecar rows by kind: tunnel 50 (tunnel table), bridge 0, level crossing 0; crossing line pairs: tunnel 112.
- Copies: 1 group doubled by evidence (line D3 over G18, the tunnels (111,171) and (111,172); extra track pieces 110,171r 110,172u 109,172l); the other short parallel pairs are not over a structure (psg197-gaps.md).

## Tunnel table (50 rows)

| # | entrance | exit | middle | separator line | group | entrance signal (type@tiles before) | crossing lines over it (tiles) | copy |
|---|---|---|---|---|---|---|---|---|
| 1 | 85,152 | 85,149 | 2 | E23 | G01 (x1) | blo@1 | NO1(2) | none |
| 2 | 95,158 | 95,172 | 13 | A1c | G02 (x3) | exi@2 | B2b(2), B2c(2), C23c(3), F1(1), F1b(1), F3(1), F3b(1) | none |
| 3 | 96,158 | 96,172 | 13 | A1d | G02 (x3) | exi@1 | B2b(2), B2c(2), C23c(2), F1(2), F1b(2), F2(1), F2b(1) | none |
| 4 | 97,158 | 97,172 | 13 | A1e | G02 (x3) | exi@2 | B2b(2), B2c(2), C23c(2), F1(1), F1b(1), F2(2), F2b(2) | none |
| 5 | 88,159 | 88,155 | 3 | C12 | G03 (x2) | blo@1 | C23(2) | none |
| 6 | 89,159 | 89,155 | 3 | E1b | G03 (x2) | blo@1 | C23(2) | none |
| 7 | 84,161 | 84,158 | 2 | E3b | G04 (x1) | blo@1 | E12(2) | none |
| 8 | 91,161 | 91,174 | 12 | A2d | G05 (x3) | exi@2 | C12d(2), C23c(2), F2(2), F2b(1) | none |
| 9 | 92,161 | 92,174 | 12 | A2b | G05 (x3) | exi@1 | C12d(1), C23c(2), F2(1), F2b(1), F3(1), F3b(1) | none |
| 10 | 93,161 | 93,174 | 12 | A2c | G05 (x3) | exi@2 | C12d(1), C23c(2), F2(1), F2b(1), F3(2), F3b(2) | none |
| 11 | 99,162 | 101,162 | 1 | B2b | G06 (x2) | exi@7 | B1b(1) | none |
| 12 | 82,163 | 82,160 | 2 | E2 | G07 (x1) | blo@2 | E2b(2) | none |
| 13 | 90,163 | 94,163 | 3 | F1b | G08 (x2) | exi@1 | C23c(2) | none |
| 14 | 99,163 | 101,163 | 1 | B2c | G06 (x2) | none | B1b(1) | none |
| 15 | 90,164 | 94,164 | 3 | F1 | G08 (x2) | exi@1 | C23c(2) | none |
| 16 | 105,164 | 108,164 | 2 | F12b | G09 (x1) | exi@1 | B2c(2) | none |
| 17 | 99,165 | 102,165 | 2 | F12 | G10 (x1) | blo@1 | B1b(1), BF1(1) | none |
| 18 | 93,166 | 96,166 | 2 | F2b | G11 (x2) | exi@1 | C23c(2) | none |
| 19 | 87,167 | 87,161 | 5 | C12b | G12 (x2) | exi@1 | E1b(2), F1(1), F1c(1), F2c(1) | none |
| 20 | 88,167 | 88,161 | 5 | C12 | G12 (x2) | exi@1 | E1b(2), F1(1), F1b(2), F2c(1) | none |
| 21 | 93,167 | 96,167 | 2 | F2 | G11 (x2) | exi@1 | C23c(1) | none |
| 22 | 102,168 | 105,168 | 2 | F3 | G13 (x1) | blo@1 | BF12(2) | none |
| 23 | 95,169 | 99,169 | 3 | F3 | G14 (x2) | blo@1 | C23c(2) | none |
| 24 | 84,170 | 84,164 | 5 | E3b | G15 (x2) | blo@1 | E1b(2), F1c(2), F2c(2) | none |
| 25 | 85,170 | 85,164 | 5 | E3 | G15 (x2) | blo@1 | E1b(2), F1c(2), F2c(2) | none |
| 26 | 95,170 | 99,170 | 3 | F3b | G14 (x2) | blo@1 | C23c(2) | none |
| 27 | 81,171 | 81,166 | 4 | E2b | G16 (x2) | exi@1 | E1(1), E1c(1), F1c(2) | none |
| 28 | 82,171 | 82,166 | 4 | E2 | G16 (x2) | exi@1 | E1(2), E1b(1), F1c(1) | none |
| 29 | 88,171 | 92,171 | 3 | F3 | G17 (x2) | blo@1 | C12d(2) | none |
| 30 | 111,171 | 107,171 | 3 | C2b | G18 (x2) | exi@1 | D3(2) | D3 doubled over it (110,171r) |
| 31 | 88,172 | 92,172 | 3 | F3b | G17 (x2) | blo@1 | C12d(2) | none |
| 32 | 111,172 | 107,172 | 3 | C2 | G18 (x2) | exi@1 | D3(2) | D3 doubled over it (110,172u, 109,172l) |
| 33 | 96,174 | 96,181 | 6 | A1c | G19 (x3) | blo@2 | C12d(1), D12c(2), D23c(1), D23d(1), D23e(2) | none |
| 34 | 97,174 | 97,181 | 6 | A1d | G19 (x3) | blo@2 | C12d(1), D12c(2), D23b(2), D23c(1) | none |
| 35 | 98,174 | 98,181 | 6 | A1e | G19 (x3) | blo@2 | C12d(1), D12c(1), D23b(1), D23c(1) | none |
| 36 | 107,174 | 103,174 | 3 | C1b | G20 (x2) | pat@2 | D23(2) | none |
| 37 | 117,174 | 110,174 | 6 | C1b | G21 (x2) | exi@1 | D2(2) | none |
| 38 | 107,175 | 103,175 | 3 | C1 | G20 (x2) | pat@2 | D23(2) | none |
| 39 | 117,175 | 110,175 | 6 | C1 | G21 (x2) | exi@1 | D2(5) | none |
| 40 | 92,176 | 92,179 | 2 | A2b | G22 (x1) | blo@1 | D23d(2) | none |
| 41 | 94,176 | 94,184 | 7 | A2c | G23 (x1) | blo@2 | D123(2), D23d(1), D23e(2) | none |
| 42 | 84,177 | 84,172 | 4 | E3b | G24 (x2) | exi@3 | F3(2), F3b(2) | none |
| 43 | 85,177 | 85,172 | 4 | E3 | G24 (x2) | exi@3 | F3(2), F3b(2) | none |
| 44 | 103,178 | 100,178 | 2 | D12c | G25 (x1) | exi@1 | D23c(2) | none |
| 45 | 92,181 | 92,183 | 1 | A2b | G26 (x2) | blo@1 | D123(1) | none |
| 46 | 93,181 | 93,183 | 1 | D23e | G26 (x2) | exi@6 | D123(1) | none |
| 47 | 97,183 | 97,187 | 3 | A1d | G27 (x2) | blo@1 | D12e(1), D12h(1) | none |
| 48 | 98,183 | 98,187 | 3 | A1e | G27 (x2) | blo@1 | D12e(1), D12f(2) | none |
| 49 | 100,184 | 100,186 | 1 | D12d | G28 (x1) | none | D12e(1) | none |
| 50 | 94,187 | 94,190 | 2 | A2c | G29 (x1) | blo@1 | AD12(2) | none |

## Crossing pairs from tile evidence

Separator is the first line, the crosser the second; "notation" says whether `ParsePlanarTopology` derives this line pair as a crossing from psg197.txt.

| separator line | crossing line | kind | middle tiles | tunnel entrances | group | copy | derived by notation |
|---|---|---|---|---|---|---|---|
| A1c | B2b | tunnel | 13 | 95,158 | G02 | none | no |
| A1c | B2c | tunnel | 13 | 95,158 | G02 | none | no |
| A1c | C12d | tunnel | 6 | 96,174 | G19 | none | no |
| A1c | C23c | tunnel | 13 | 95,158 | G02 | none | no |
| A1c | D12c | tunnel | 6 | 96,174 | G19 | none | no |
| A1c | D23c | tunnel | 6 | 96,174 | G19 | none | no |
| A1c | D23d | tunnel | 6 | 96,174 | G19 | none | yes |
| A1c | D23e | tunnel | 6 | 96,174 | G19 | none | no |
| A1c | F1 | tunnel | 13 | 95,158 | G02 | none | no |
| A1c | F1b | tunnel | 13 | 95,158 | G02 | none | no |
| A1c | F3 | tunnel | 13 | 95,158 | G02 | none | no |
| A1c | F3b | tunnel | 13 | 95,158 | G02 | none | no |
| A1d | B2b | tunnel | 13 | 96,158 | G02 | none | no |
| A1d | B2c | tunnel | 13 | 96,158 | G02 | none | no |
| A1d | C12d | tunnel | 6 | 97,174 | G19 | none | no |
| A1d | C23c | tunnel | 13 | 96,158 | G02 | none | no |
| A1d | D12c | tunnel | 6 | 97,174 | G19 | none | no |
| A1d | D12e | tunnel | 3 | 97,183 | G27 | none | no |
| A1d | D12h | tunnel | 3 | 97,183 | G27 | none | no |
| A1d | D23b | tunnel | 6 | 97,174 | G19 | none | no |
| A1d | D23c | tunnel | 6 | 97,174 | G19 | none | no |
| A1d | F1 | tunnel | 13 | 96,158 | G02 | none | no |
| A1d | F1b | tunnel | 13 | 96,158 | G02 | none | no |
| A1d | F2 | tunnel | 13 | 96,158 | G02 | none | no |
| A1d | F2b | tunnel | 13 | 96,158 | G02 | none | no |
| A1e | B2b | tunnel | 13 | 97,158 | G02 | none | no |
| A1e | B2c | tunnel | 13 | 97,158 | G02 | none | no |
| A1e | C12d | tunnel | 6 | 98,174 | G19 | none | no |
| A1e | C23c | tunnel | 13 | 97,158 | G02 | none | no |
| A1e | D12c | tunnel | 6 | 98,174 | G19 | none | no |
| A1e | D12e | tunnel | 3 | 98,183 | G27 | none | no |
| A1e | D12f | tunnel | 3 | 98,183 | G27 | none | no |
| A1e | D23b | tunnel | 6 | 98,174 | G19 | none | no |
| A1e | D23c | tunnel | 6 | 98,174 | G19 | none | no |
| A1e | F1 | tunnel | 13 | 97,158 | G02 | none | no |
| A1e | F1b | tunnel | 13 | 97,158 | G02 | none | no |
| A1e | F2 | tunnel | 13 | 97,158 | G02 | none | no |
| A1e | F2b | tunnel | 13 | 97,158 | G02 | none | no |
| A2b | C12d | tunnel | 12 | 92,161 | G05 | none | no |
| A2b | C23c | tunnel | 12 | 92,161 | G05 | none | no |
| A2b | D123 | tunnel | 1 | 92,181 | G26 | none | no |
| A2b | D23d | tunnel | 2 | 92,176 | G22 | none | no |
| A2b | F2 | tunnel | 12 | 92,161 | G05 | none | no |
| A2b | F2b | tunnel | 12 | 92,161 | G05 | none | no |
| A2b | F3 | tunnel | 12 | 92,161 | G05 | none | no |
| A2b | F3b | tunnel | 12 | 92,161 | G05 | none | no |
| A2c | AD12 | tunnel | 2 | 94,187 | G29 | none | yes |
| A2c | C12d | tunnel | 12 | 93,161 | G05 | none | no |
| A2c | C23c | tunnel | 12 | 93,161 | G05 | none | no |
| A2c | D123 | tunnel | 7 | 94,176 | G23 | none | no |
| A2c | D23d | tunnel | 7 | 94,176 | G23 | none | no |
| A2c | D23e | tunnel | 7 | 94,176 | G23 | none | yes |
| A2c | F2 | tunnel | 12 | 93,161 | G05 | none | no |
| A2c | F2b | tunnel | 12 | 93,161 | G05 | none | no |
| A2c | F3 | tunnel | 12 | 93,161 | G05 | none | no |
| A2c | F3b | tunnel | 12 | 93,161 | G05 | none | no |
| A2d | C12d | tunnel | 12 | 91,161 | G05 | none | no |
| A2d | C23c | tunnel | 12 | 91,161 | G05 | none | no |
| A2d | F2 | tunnel | 12 | 91,161 | G05 | none | no |
| A2d | F2b | tunnel | 12 | 91,161 | G05 | none | no |
| B2b | B1b | tunnel | 1 | 99,162 | G06 | none | no |
| B2c | B1b | tunnel | 1 | 99,163 | G06 | none | no |
| C1 | D2 | tunnel | 6 | 117,175 | G21 | none | no |
| C1 | D23 | tunnel | 3 | 107,175 | G20 | none | no |
| C12 | C23 | tunnel | 3 | 88,159 | G03 | none | no |
| C12 | E1b | tunnel | 5 | 88,167 | G12 | none | no |
| C12 | F1 | tunnel | 5 | 88,167 | G12 | none | no |
| C12 | F1b | tunnel | 5 | 88,167 | G12 | none | no |
| C12 | F2c | tunnel | 5 | 88,167 | G12 | none | no |
| C12b | E1b | tunnel | 5 | 87,167 | G12 | none | no |
| C12b | F1 | tunnel | 5 | 87,167 | G12 | none | no |
| C12b | F1c | tunnel | 5 | 87,167 | G12 | none | no |
| C12b | F2c | tunnel | 5 | 87,167 | G12 | none | no |
| C1b | D2 | tunnel | 6 | 117,174 | G21 | none | no |
| C1b | D23 | tunnel | 3 | 107,174 | G20 | none | no |
| C2 | D3 | tunnel | 3 | 111,172 | G18 | D3 doubled | no |
| C2b | D3 | tunnel | 3 | 111,171 | G18 | D3 doubled | yes |
| D12c | D23c | tunnel | 2 | 103,178 | G25 | none | no |
| D12d | D12e | tunnel | 1 | 100,184 | G28 | none | no |
| D23e | D123 | tunnel | 1 | 93,181 | G26 | none | no |
| E1b | C23 | tunnel | 3 | 89,159 | G03 | none | yes |
| E2 | E1 | tunnel | 4 | 82,171 | G16 | none | no |
| E2 | E1b | tunnel | 4 | 82,171 | G16 | none | no |
| E2 | E2b | tunnel | 2 | 82,163 | G07 | none | no |
| E2 | F1c | tunnel | 4 | 82,171 | G16 | none | no |
| E23 | NO1 | tunnel | 2 | 85,152 | G01 | none | no |
| E2b | E1 | tunnel | 4 | 81,171 | G16 | none | no |
| E2b | E1c | tunnel | 4 | 81,171 | G16 | none | no |
| E2b | F1c | tunnel | 4 | 81,171 | G16 | none | no |
| E3 | E1b | tunnel | 5 | 85,170 | G15 | none | no |
| E3 | F1c | tunnel | 5 | 85,170 | G15 | none | no |
| E3 | F2c | tunnel | 5 | 85,170 | G15 | none | no |
| E3 | F3 | tunnel | 4 | 85,177 | G24 | none | no |
| E3 | F3b | tunnel | 4 | 85,177 | G24 | none | no |
| E3b | E12 | tunnel | 2 | 84,161 | G04 | none | no |
| E3b | E1b | tunnel | 5 | 84,170 | G15 | none | no |
| E3b | F1c | tunnel | 5 | 84,170 | G15 | none | no |
| E3b | F2c | tunnel | 5 | 84,170 | G15 | none | no |
| E3b | F3 | tunnel | 4 | 84,177 | G24 | none | no |
| E3b | F3b | tunnel | 4 | 84,177 | G24 | none | no |
| F1 | C23c | tunnel | 3 | 90,164 | G08 | none | no |
| F12 | B1b | tunnel | 2 | 99,165 | G10 | none | yes |
| F12 | BF1 | tunnel | 2 | 99,165 | G10 | none | no |
| F12b | B2c | tunnel | 2 | 105,164 | G09 | none | yes |
| F1b | C23c | tunnel | 3 | 90,163 | G08 | none | yes |
| F2 | C23c | tunnel | 2 | 93,167 | G11 | none | yes |
| F2b | C23c | tunnel | 2 | 93,166 | G11 | none | no |
| F3 | BF12 | tunnel | 2 | 102,168 | G13 | none | yes |
| F3 | C12d | tunnel | 3 | 88,171 | G17 | none | no |
| F3 | C23c | tunnel | 3 | 95,169 | G14 | none | no |
| F3b | C12d | tunnel | 3 | 88,172 | G17 | none | no |
| F3b | C23c | tunnel | 3 | 95,170 | G14 | none | no |

## Line index

Every line of psg197.txt with its routes, start and end tile (node piece, or port edge cell) and the tunnels (entrance tiles) it carries in series. Port lines AB1.. CD1.. EF1.. in, NO1.. EO1.. SO1.. out; an in port line that ends at a split on the edge cell has length 0.

| line | routes | from | to | tunnels in series |
|---|---|---|---|---|
| A1 | A1 | 93,148 | 95,154 | - |
| A1b | A1 | 95,154 | 96,155 | - |
| A1c | A1 | 95,154 | 96,187 | 95,158 96,174 |
| A1d | A1 | 96,155 | 97,192 | 96,158 97,174 97,183 |
| A1e | A1 | 96,155 | 98,191 | 97,158 98,174 98,183 |
| A2 | A2 | 91,147 | 92,158 | - |
| A2b | A2 | 92,158 | 92,186 | 92,161 92,176 92,181 |
| A2c | A2 | 92,158 | 94,195 | 93,161 94,176 94,187 |
| A2d | A2 | 92,158 | 88,182 | 91,161 |
| AB1 | A1+B1 | port N91 | 93,148 | - |
| AB2 | A2+B2 | port N90 | 91,147 | - |
| AD12 | A1+D1+D2 | 96,187 | 92,193 | - |
| AD12b | A1+D1+D2 | 98,191 | 97,192 | - |
| AD12c | A1+D1+D2 | 97,192 | 94,195 | - |
| AD23 | A2+D2+D3 | 88,182 | 88,185 | - |
| AD23b | A2+D2+D3 | 92,186 | 92,193 | - |
| B1 | B1 | 93,148 | 98,155 | - |
| B12 | B1+B2 | 104,162 | 111,164 | - |
| B1b | B1 | 98,155 | 100,165 | - |
| B1c | B1 | 98,155 | 104,162 | - |
| B2 | B2 | 91,147 | 94,158 | - |
| B2b | B2 | 94,158 | 104,162 | 99,162 |
| B2c | B2 | 94,158 | 112,166 | 99,163 |
| BF1 | B1+F1 | 100,165 | 102,166 | - |
| BF12 | B1+F1+F2 | 102,166 | 105,169 | - |
| BF12b | B2+F1+F2 | 112,166 | 113,167 | - |
| C1 | C1 | 120,174 | 99,175 | 107,175 117,175 |
| C12 | C1+C2 | 88,170 | 88,149 | 88,159 88,167 |
| C12b | C1+C2 | 88,170 | 86,159 | 87,167 |
| C12c | C1+C2 | 100,174 | 99,175 | - |
| C12d | C1+C2 | 99,175 | 88,170 | - |
| C1b | C1 | 120,174 | 100,174 | 107,174 117,174 |
| C1c | C1 | 123,174 | 120,174 | - |
| C2 | C2 | 114,172 | 100,174 | 111,172 |
| C23 | C2+C3 | 90,159 | 86,151 | - |
| C23b | C2+C3 | 90,159 | 89,150 | - |
| C23c | C2+C3 | 98,171 | 90,159 | - |
| C2b | C2 | 114,172 | 98,171 | 111,171 |
| C2c | C2 | 128,173 | 114,172 | - |
| C3 | C3 | 118,170 | 98,171 | - |
| CD1 | C1+D1 | port E174 | 123,174 | - |
| CD2 | C2+D2 | port E173 | 128,173 | - |
| CD3 | C3+D3 | port E169 | 118,170 | - |
| CE123 | C1+C2+C3+E1 | 88,149 | 85,146 | - |
| CE123b | C2+C3+E1 | 89,150 | 88,149 | - |
| CE123c | C1+C2+E1+E2+E3 | 86,157 | 86,151 | - |
| CE123d | C1+C2+E3 | 86,159 | 86,157 | - |
| D1 | D1 | 123,174 | 110,176 | - |
| D12 | D1+D2 | 110,176 | 106,178 | - |
| D123 | D1+D2+D3 | 95,180 | 88,185 | - |
| D12b | D1+D2 | 106,178 | 105,179 | - |
| D12c | D1+D2 | 106,178 | 95,180 | 103,178 |
| D12d | D1+D2 | 104,179 | 98,191 | 100,184 |
| D12e | D1+D2 | 104,179 | 97,185 | - |
| D12f | D1+D2 | 105,179 | 97,185 | - |
| D12g | D1+D2 | 105,179 | 104,179 | - |
| D12h | D1+D2 | 97,185 | 96,187 | - |
| D2 | D2 | 117,173 | 110,176 | - |
| D23 | D2+D3 | 107,173 | 104,176 | - |
| D23b | D2+D3 | 104,176 | 97,177 | - |
| D23c | D2+D3 | 104,176 | 95,180 | - |
| D23d | D2+D3 | 97,177 | 88,182 | - |
| D23e | D2+D3 | 97,177 | 92,186 | 93,181 |
| D2b | D2 | 117,173 | 107,173 | - |
| D2c | D2 | 128,173 | 117,173 | - |
| D3 | D3 | 118,170 | 107,173 | - |
| E1 | E1 | 81,168 | 83,160 | - |
| E12 | E1+E2 | 83,160 | 86,157 | - |
| E1b | E1 | 81,168 | 89,150 | 89,159 |
| E1c | E1 | 78,173 | 81,168 | - |
| E2 | E2 | 81,174 | 84,155 | 82,163 82,171 |
| E23 | E2+E3 | 84,155 | 85,146 | 85,152 |
| E2b | E2 | 81,174 | 83,160 | 81,171 |
| E2c | E2 | 81,176 | 81,174 | - |
| E3 | E3 | 84,182 | 86,159 | 85,170 85,177 |
| E3b | E3 | 84,182 | 84,155 | 84,161 84,170 84,177 |
| E3c | E3 | 84,184 | 84,182 | - |
| EF1 | E1+F1 | port S81 | 78,173 | - |
| EF2 | E2+F2 | port S82 | 81,176 | - |
| EF3 | E3+F3 | port S83 | 84,184 | - |
| EO1 | B1+B2+F1+F2 | 111,164 | port E164 | - |
| EO2 | B2+F1+F2+F3 | 113,167 | port E166 | - |
| EO3 | B1+F1+F2+F3 | 105,169 | port E167 | - |
| F1 | F1 | 87,164 | 98,165 | 90,164 |
| F12 | F1+F2 | 98,165 | 103,165 | 99,165 |
| F12b | F1+F2 | 103,165 | 111,164 | 105,164 |
| F12c | F1+F2 | 103,165 | 112,166 | - |
| F1b | F1 | 87,164 | 100,165 | 90,163 |
| F1c | F1 | 78,173 | 87,164 | - |
| F2 | F2 | 90,166 | 102,166 | 93,167 |
| F2b | F2 | 90,166 | 98,165 | 93,166 |
| F2c | F2 | 81,176 | 90,166 | - |
| F3 | F3 | 82,178 | 113,167 | 102,168 95,169 88,171 |
| F3b | F3 | 82,178 | 105,169 | 95,170 88,172 |
| F3c | F3 | 84,184 | 82,178 | - |
| NO1 | C1+C2+C3+E1+E2+E3 | 86,151 | port N84 | - |
| NO2 | C1+C2+C3+E1+E2+E3 | 85,146 | port N85 | - |
| SO1 | A1+A2+D1+D2 | 94,195 | port S90 | - |
| SO2 | A1+A2+D1+D2+D3 | 92,193 | port S89 | - |
| SO3 | A2+D1+D2+D3 | 88,185 | port S88 | - |
