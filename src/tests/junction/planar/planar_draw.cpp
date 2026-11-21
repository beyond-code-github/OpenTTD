/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_draw.cpp Tests of the drawing program, the feasibility oracle of Shape's shape (E5). */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/junction_spec.h"
#include "../../../junction/planar/planar_draw.h"
#include "../../../junction/planar/planar_model.h"
#include "../../../junction/planar/planar_shape_layer.h"
#include "../../../junction/planar/planar_structure.h"
#include "../../../junction/planar/planar_topology.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "../../../3rdparty/fmt/format.h"

#include "../../../safeguards.h"

/** The shaped first variant of a hub, built through Structure and Shape. */
struct DrawCase {
	PlanarTopology topology;
	JunctionSpec spec = MakeTestJunctionSpec();
	PlanarShapeVariant v;
};

/**
 * Build and shape the first Structure variant of a topology text.
 * @param text The topology text.
 * @param keep How many Structure variants to keep.
 * @return The case with its shaped variant.
 */
static DrawCase ShapeFirst(const std::string &text, size_t keep)
{
	DrawCase c;
	std::string error;
	c.topology = ParsePlanarTopology(text, &error);
	REQUIRE(error.empty());
	const PlanarStructureResult r = BuildPlanarStructure(c.topology, c.spec, keep);
	INFO("why: " << r.why);
	REQUIRE(!r.variants.empty());
	std::string why;
	const bool ok = ShapePlanarStructureVariant(r.variants[0], c.v, why);
	INFO("why: " << why);
	REQUIRE(ok);
	return c;
}

/** Whether any conflict tag starts with a family name. */
static bool HasFamily(const PlanarDrawResult &r, std::string_view family)
{
	return std::ranges::any_of(r.conflict, [&](const std::string &tag) { return tag.starts_with(family); });
}

TEST_CASE("planar draw: the one-lane T is drawable", "[junction][planar][planardraw]")
{
	const DrawCase c = ShapeFirst(PlanarTText(1), 1);
	/* Without clearance: rows and copies need the corridor family (CARD-E5). */
	const PlanarDrawResult r = CheckPlanarDrawable(c.v, 5.0, {.clearance = false});
	std::string conflicts;
	for (const std::string &tag : r.conflict) conflicts += tag + "; ";
	INFO("status " << r.status << " conflict " << conflicts);
	REQUIRE(r.feasible);

	const PlanarTrackMap &map = c.v.structure.map;
	REQUIRE(r.points.size() == map.tracks.size());
	int width = 0;
	int height = 0;
	for (const auto &track : r.points) {
		for (const auto &p : track) {
			width = std::max(width, p[0]);
			height = std::max(height, p[1]);
		}
	}
	for (size_t t = 0; t < map.tracks.size(); t++) {
		const std::vector<int> &pieces = c.v.tracks[t].pieces;
		REQUIRE(r.points[t].size() == pieces.size() + 1);
		for (size_t i = 0; i < pieces.size(); i++) {
			/* The piece runs from its start along its heading by a whole number of units, never backwards. */
			const std::array<int, 2> step = PlanarModel::STEP[((pieces[i] % 8) + 8) % 8];
			const int dx = r.points[t][i + 1][0] - r.points[t][i][0];
			const int dy = r.points[t][i + 1][1] - r.points[t][i][1];
			INFO("track " << map.tracks[t].name << " piece " << i);
			CHECK(dx * step[1] - dy * step[0] == 0);
			CHECK(dx * step[0] + dy * step[1] >= 0);
		}
	}
	for (const PlanarPort &port : map.expanded.ports) {
		for (const uint32_t line : port.lines) {
			const std::vector<std::array<int, 2>> &points = r.points[line];
			const std::array<int, 2> &p = port.in ? points.front() : points.back();
			INFO("port line " << map.tracks[line].name);
			switch (port.side) {
				case PlanarSide::N: CHECK(p[1] == 0); break;
				case PlanarSide::S: CHECK(p[1] == height); break;
				case PlanarSide::W: CHECK(p[0] == 0); break;
				case PlanarSide::E: CHECK(p[0] == width); break;
			}
		}
	}
}

TEST_CASE("planar draw: the two-lane T's best variant is drawable", "[junction][planar][planardraw]")
{
	const DrawCase c = ShapeFirst(PlanarTText(2), 16);
	/* Without clearance: rows and copies need the corridor family (CARD-E5). */
	const PlanarDrawResult r = CheckPlanarDrawable(c.v, 5.0, {.clearance = false});
	std::string conflicts;
	for (const std::string &tag : r.conflict) conflicts += tag + "; ";
	INFO("status " << r.status << " conflict " << conflicts);
	CHECK(r.feasible);
	WARN(fmt::format("T2 draw: feasible {}, rows {}, columns {}, {:.0f} ms", r.feasible, r.rows, r.columns, r.ms));
}

/** Read a file of tools/junction/topologies. */
static std::string ReadTopologyFileForDraw(const std::string &name)
{
	std::ifstream file(std::string(JUNCTION_TOPOLOGY_DIR) + "/" + name);
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

TEST_CASE("planar draw: PSG 197 (Shape's own shape)", "[.][planarpsg197]")
{
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(ReadTopologyFileForDraw("psg197.txt"), &error);
	REQUIRE(error.empty());
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::vector<std::string> report;
	const PlanarStructureVariant structure = LoadPlanarStructureVariant(topology, ReadTopologyFileForDraw("psg197.structure.txt"), spec, report, &error);
	REQUIRE(error.empty());
	PlanarShapeVariant shaped;
	std::string why;
	REQUIRE(ShapePlanarStructureVariant(structure, shaped, why));
	for (const bool clear_rows : {true, false}) {
		const std::string label = clear_rows ? "197 draw" : "197 draw (landings, no clearance rows)";
		const PlanarDrawResult r = CheckPlanarDrawable(shaped, 15.0, {.clearance = true, .clear_rows = clear_rows});
		WARN(fmt::format("{}: feasible {}, status {}, rows {}, columns {}, {:.0f} ms", label, r.feasible, r.status, r.rows, r.columns, r.ms));
		std::map<std::string, size_t> families;
		for (const std::string &tag : r.conflict) families[tag.substr(0, tag.find(' '))]++;
		for (const auto &[family, count] : families) WARN(fmt::format("{} conflicts: {} {}", label, family, count));
		for (size_t i = 0; i < std::min<size_t>(20, r.conflict.size()); i++) WARN(fmt::format("{} conflict {}: {}", label, i, r.conflict[i]));
	}
	size_t on_dummy = 0, on_track = 0, on_box = 0;
	for (const PlanarShapeDummy &d : shaped.dummies) {
		if (d.land_track == PLANAR_DUMMY_TRACK) {
			on_dummy++;
		} else if (d.land_track == UINT32_MAX) {
			on_box++;
		} else {
			on_track++;
		}
	}
	WARN(fmt::format("197 dummies {}: on dummies {}, on tracks {}, on the box {}; cells {}", shaped.dummies.size(), on_dummy, on_track, on_box, PlanarShapeCellWalls(shaped).size()));
}

TEST_CASE("planar draw: a crossing moved to a piece it cannot meet is a cross conflict", "[junction][planar][planardraw]")
{
	DrawCase c = ShapeFirst(PlanarTText(1), 1);
	const PlanarTrackMap &map = c.v.structure.map;
	/* The one-crosser structure's separator F1 carries the crossing with B1 on a piece heading south-east or south; its piece 1 heads east, parallel to the crosser's west-heading piece. */
	REQUIRE(c.v.structure.structures.size() == 2);
	const PlanarStructure &structure = c.v.structure.structures[1];
	REQUIRE(structure.crossings.size() == 1);
	const uint32_t separator = structure.separator;
	const auto at = std::ranges::find(map.events[separator], structure.crossings[0]);
	REQUIRE(at != map.events[separator].end());
	REQUIRE(c.v.tracks[separator].pieces.size() > 1);
	REQUIRE(c.v.tracks[separator].pieces[1] % 8 == 2);
	c.v.tracks[separator].event_piece[at - map.events[separator].begin()] = 1;
	const PlanarDrawResult r = CheckPlanarDrawable(c.v, 5.0);
	CHECK(!r.feasible);
	CHECK(r.status == "Infeasible");
	CHECK(HasFamily(r, "cross "));
}

TEST_CASE("planar draw: two ports on a side with no gap are a clearance conflict", "[junction][planar][planardraw]")
{
	DrawCase c = ShapeFirst(PlanarTText(1), 1);
	c.v.structure.side_gap = {0, 0, 0, 0};
	const PlanarDrawResult r = CheckPlanarDrawable(c.v, 5.0);
	std::string conflicts;
	for (const std::string &tag : r.conflict) conflicts += tag + "; ";
	INFO("status " << r.status << " conflict " << conflicts);
	CHECK(!r.feasible);
	CHECK((HasFamily(r, "clear ") || HasFamily(r, "port ") || HasFamily(r, "box")));
}

TEST_CASE("planar draw: a dummy that cannot reach its landing piece is a land conflict", "[junction][planar][planardraw]")
{
	DrawCase c = ShapeFirst(PlanarTText(1), 1);
	const PlanarDrawResult base = CheckPlanarDrawable(c.v, 5.0, {.clearance = false});
	REQUIRE(base.feasible);
	bool found = false;
	for (size_t di = 0; di < c.v.dummies.size() && !found; di++) {
		const uint32_t land_track = c.v.dummies[di].land_track;
		if (land_track == UINT32_MAX || land_track == PLANAR_DUMMY_TRACK) continue;
		const uint32_t original = c.v.dummies[di].land_piece;
		for (uint32_t piece = 0; piece < c.v.tracks[land_track].pieces.size() && !found; piece++) {
			if (piece == original) continue;
			c.v.dummies[di].land_piece = piece;
			const PlanarDrawResult r = CheckPlanarDrawable(c.v, 5.0);
			if (!r.feasible && HasFamily(r, "land ")) {
				found = true;
				WARN(fmt::format("land test: dummy {} landing piece {} -> {} is infeasible", di, original, piece));
			}
		}
		c.v.dummies[di].land_piece = original;
	}
	CHECK(found);
}

TEST_CASE("planar draw: face walls are the cells' walls when there are no dummies", "[junction][planar][planardraw]")
{
	DrawCase c = ShapeFirst(PlanarTText(1), 1);
	c.v.dummies.clear();
	const std::vector<PlanarFaceWalls> faces = PlanarShapeFaceWalls(c.v);
	const std::vector<std::vector<PlanarCellWall>> cells = PlanarShapeCellWalls(c.v);
	REQUIRE(faces.size() == cells.size());
	for (size_t i = 0; i < faces.size(); i++) {
		if (i > 0) CHECK(faces[i].face > faces[i - 1].face);
		REQUIRE(faces[i].walls.size() == cells[i].size());
		for (size_t k = 0; k < cells[i].size(); k++) {
			CHECK(faces[i].walls[k].track == cells[i][k].track);
			CHECK(faces[i].walls[k].piece == cells[i][k].piece);
			CHECK(faces[i].walls[k].heading == cells[i][k].heading);
		}
	}
}

TEST_CASE("planar draw: pairwise mode writes separations and no landings", "[junction][planar][planardraw]")
{
	const DrawCase c = ShapeFirst(PlanarTText(1), 1);
	const PlanarDrawResult r = CheckPlanarDrawable(c.v, 1.0, {.pairwise = true});
	WARN(fmt::format("T1 pairwise: status {}, pairs {}, binaries {}, rows {}, columns {}", r.status, r.pairs, r.binaries, r.rows, r.columns));
	CHECK(r.pairs > 0);
	CHECK(r.binaries >= 2 * r.pairs);
	CHECK(r.binaries <= 4 * r.pairs);
}

/** The shaped PSG 197 variant. */
static PlanarShapeVariant Shaped197()
{
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(ReadTopologyFileForDraw("psg197.txt"), &error);
	REQUIRE(error.empty());
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::vector<std::string> report;
	const PlanarStructureVariant structure = LoadPlanarStructureVariant(topology, ReadTopologyFileForDraw("psg197.structure.txt"), spec, report, &error);
	REQUIRE(error.empty());
	PlanarShapeVariant shaped;
	std::string why;
	REQUIRE(ShapePlanarStructureVariant(structure, shaped, why));
	return shaped;
}

/** Warn one KA line for a solve; returns the result's feasibility. */
static void WarnKa(const std::string &label, const PlanarDrawResult &r)
{
	WARN(fmt::format("KA {}: feasible {}, status {}, pairs {}, binaries {}, rows {}, columns {}, {:.0f} ms", label, r.feasible, r.status, r.pairs, r.binaries, r.rows, r.columns, r.ms));
	if (r.feasible) return;
	std::map<std::string, size_t> families;
	for (const std::string &tag : r.conflict) families[tag.substr(0, tag.find(' '))]++;
	for (const auto &[family, count] : families) WARN(fmt::format("KA {} conflicts: {} {}", label, family, count));
	for (size_t i = 0; i < std::min<size_t>(20, r.conflict.size()); i++) WARN(fmt::format("KA {} conflict {}: {}", label, i, r.conflict[i]));
}

TEST_CASE("planar draw: PSG 197 pairwise, faces 16 and 17 alone are drawable", "[.][planarkeepapart]")
{
	const PlanarShapeVariant shaped = Shaped197();
	for (const uint32_t f : {16u, 17u}) {
		const PlanarDrawResult r = CheckPlanarDrawable(shaped, 15.0, {.clearance = true, .clear_rows = true, .pairwise = true, .only_faces = {f}});
		WarnKa(fmt::format("face {}", f), r);
		CHECK(r.feasible);
	}
}

TEST_CASE("planar draw: PSG 197 pairwise, every face alone", "[.][planarkeepapart]")
{
	const PlanarShapeVariant shaped = Shaped197();
	const auto begin = std::chrono::steady_clock::now();
	size_t feasible = 0, infeasible = 0, limit = 0;
	std::string bad;
	for (const PlanarFaceWalls &f : PlanarShapeFaceWalls(shaped)) {
		if (std::chrono::steady_clock::now() - begin > std::chrono::minutes(15)) {
			WARN("KA sweep stopped: 15 min wall passed");
			break;
		}
		const PlanarDrawResult r = CheckPlanarDrawable(shaped, 5.0, {.clearance = true, .clear_rows = true, .pairwise = true, .only_faces = {f.face}});
		if (r.feasible) {
			feasible++;
			WARN(fmt::format("KA face {}: feasible {}, status {}, pairs {}, binaries {}, rows {}, columns {}, {:.0f} ms", f.face, r.feasible, r.status, r.pairs, r.binaries, r.rows, r.columns, r.ms));
			continue;
		}
		(r.status == "Infeasible" ? infeasible : limit)++;
		bad += fmt::format(" {}{}", f.face, r.status == "Infeasible" ? "" : "(limit)");
		WarnKa(fmt::format("face {}", f.face), r);
	}
	WARN(fmt::format("KA sweep: feasible {}, infeasible {}, limit {}; not feasible:{}", feasible, infeasible, limit, bad));
}

TEST_CASE("planar draw: PSG 197 pairwise, whole", "[.][planarkeepapart]")
{
	const PlanarShapeVariant shaped = Shaped197();
	for (const double seconds : {15.0, 60.0}) {
		const PlanarDrawResult r = CheckPlanarDrawable(shaped, seconds, {.clearance = true, .clear_rows = true, .pairwise = true});
		WarnKa(fmt::format("whole {:.0f} s", seconds), r);
	}
}

TEST_CASE("planar draw: PSG 197 by sides, every face alone and whole", "[.][planarkeepapart]")
{
	const PlanarShapeVariant shaped = Shaped197();
	const auto begin = std::chrono::steady_clock::now();
	size_t feasible = 0, infeasible = 0, undecided = 0, skipped = 0;
	for (const PlanarFaceWalls &f : PlanarShapeFaceWalls(shaped)) {
		if (std::chrono::steady_clock::now() - begin > std::chrono::minutes(20)) {
			skipped++;
			continue;
		}
		const PlanarDrawResult r = CheckPlanarDrawable(shaped, 15.0, {.only_faces = {f.face}});
		if (r.feasible) {
			feasible++;
			continue;
		}
		(r.status == "Infeasible" ? infeasible : undecided)++;
		std::string tags;
		for (size_t i = 0; i < std::min<size_t>(5, r.conflict.size()); i++) tags += " [" + r.conflict[i] + "]";
		WARN(fmt::format("FS face {}: status {}, {:.0f} ms, conflict{}", f.face, r.status, r.ms, tags));
	}
	if (skipped > 0) WARN(fmt::format("FS stopped after 20 min: {} faces skipped", skipped));
	const PlanarDrawResult whole = CheckPlanarDrawable(shaped, 60.0);
	WARN(fmt::format("FS whole: status {}, rows {}, {:.0f} ms", whole.status, whole.rows, whole.ms));
	std::map<std::string, size_t> families;
	for (const std::string &tag : whole.conflict) families[tag.substr(0, tag.find(' '))]++;
	for (const auto &[family, count] : families) WARN(fmt::format("FS whole conflicts: {} {}", family, count));
	for (size_t i = 0; i < std::min<size_t>(20, whole.conflict.size()); i++) WARN(fmt::format("FS whole conflict {}: {}", i, whole.conflict[i]));
	WARN(fmt::format("FS 197 faces: feasible {}, infeasible {}, undecided {}, skipped {}; whole: {} in {:.0f} ms", feasible, infeasible, undecided, skipped, whole.status, whole.ms));
}

TEST_CASE("planar draw: no objective still decides", "[junction][planar][planardraw]")
{
	const DrawCase c = ShapeFirst(PlanarTText(1), 1);
	const PlanarDrawResult r = CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .tidy = false});
	REQUIRE(r.feasible);
	CHECK(!r.points.empty());
}

TEST_CASE("planar draw: PSG 197 undecided faces without the objective", "[.][planarkeepapart]")
{
	const PlanarShapeVariant shaped = Shaped197();
	size_t feasible = 0, infeasible = 0, undecided = 0;
	for (const uint32_t f : {4u, 6u, 46u, 52u, 64u, 71u, 72u, 75u, 84u, 99u, 124u, 141u, 147u, 153u}) {
		const PlanarDrawResult r = CheckPlanarDrawable(shaped, 15.0, {.only_faces = {f}, .tidy = false});
		if (r.feasible) {
			feasible++;
		} else {
			(r.status == "Infeasible" ? infeasible : undecided)++;
		}
		std::string tags;
		for (size_t i = 0; i < std::min<size_t>(5, r.conflict.size()); i++) tags += " [" + r.conflict[i] + "]";
		WARN(fmt::format("FSc face {}: status {}, {:.0f} ms, conflict{}", f, r.status, r.ms, tags));
	}
	const PlanarDrawResult whole = CheckPlanarDrawable(shaped, 60.0, {.tidy = false});
	WARN(fmt::format("FSc whole: status {}, {:.0f} ms", whole.status, whole.ms));
	std::map<std::string, size_t> families;
	for (const std::string &tag : whole.conflict) families[tag.substr(0, tag.find(' '))]++;
	for (const auto &[family, count] : families) WARN(fmt::format("FSc whole conflicts: {} {}", family, count));
	WARN(fmt::format("FSc faces: feasible {}, infeasible {}, undecided {}; whole: {} in {:.0f} ms", feasible, infeasible, undecided, whole.status, whole.ms));
}

/** The hand cell of the side-row tests: a step a, insert, b on the north side, then top, left and bottom walls. */
static const std::vector<PlanarCellWall> SIDE_CELL = {{0, 0, 0}, {0, 1, 1}, {0, 2, 0}, {1, 0, 5}, {2, 0, 4}, {3, 0, 2}};
static const std::vector<bool> SIDE_INSERT = {false, true, false, false, false, false};

TEST_CASE("planar draw: clearance rows go by sides", "[junction][planar][planardraw]")
{
	using Row = std::tuple<uint32_t, uint32_t, int>;
	std::vector<Row> got;
	for (const PlanarClearRow &r : PlanarCellClearRows(SIDE_CELL, SIDE_INSERT)) got.emplace_back(r.vertex, r.wall, r.spacing);
	std::ranges::sort(got);
	const std::vector<Row> want = {{0, 3, 2}, {0, 4, 2}, {0, 5, 1}, {1, 3, 2}, {1, 4, 2}, {1, 5, 1}, {2, 4, 2}, {2, 5, 1}, {3, 0, 2}, {3, 2, 2}, {3, 5, 1}, {4, 0, 2}, {4, 2, 2}, {4, 3, 2}, {5, 3, 2}, {5, 4, 2}};
	CHECK(got == want);
	for (const auto &[vertex, wall, spacing] : got) {
		CHECK(wall != 1);
		CHECK(!(vertex <= 1 && (wall == 0 || wall == 2)));
	}
	const std::vector<PlanarClearRow> old = PlanarCellClearRows(SIDE_CELL, std::vector<bool>(6, false));
	CHECK(std::ranges::any_of(old, [](const PlanarClearRow &r) { return r.wall == 1; }));
}

/** Whether every row of the side cell holds on the vertex points; the failing rows are returned. */
static std::vector<std::pair<uint32_t, uint32_t>> FailingSideRows(const std::array<std::array<int, 2>, 6> &V)
{
	std::vector<std::pair<uint32_t, uint32_t>> failing;
	for (const PlanarClearRow &r : PlanarCellClearRows(SIDE_CELL, SIDE_INSERT)) {
		const std::array<int, 2> from = V[(r.wall + 5) % 6];
		const std::array<int, 2> normal = PlanarModel::STEP[(SIDE_CELL[r.wall].heading + 6) % 8];
		const int value = (V[r.vertex][0] - from[0]) * normal[0] + (V[r.vertex][1] - from[1]) * normal[1];
		if (value < r.spacing) failing.emplace_back(r.vertex, r.wall);
	}
	return failing;
}

TEST_CASE("planar draw: side rows refuse a step's end caps", "[junction][planar][planardraw]")
{
	CHECK(FailingSideRows({{{11, 16}, {13, 14}, {13, 10}, {7, 16}, {7, 20}, {11, 20}}}).empty());
	const auto reach = FailingSideRows({{{11, 16}, {13, 14}, {13, 14}, {9, 18}, {9, 20}, {11, 20}}});
	CHECK(std::ranges::find(reach, std::make_pair(1u, 3u)) != reach.end());
	const auto pocket = FailingSideRows({{{11, 16}, {13, 14}, {13, 10}, {12, 11}, {12, 20}, {11, 20}}});
	CHECK(std::ranges::find(pocket, std::make_pair(3u, 0u)) != pocket.end());
}

TEST_CASE("planar draw: cell faces match the cells", "[junction][planar][planardraw]")
{
	const DrawCase c = ShapeFirst(PlanarTText(1), 1);
	const std::vector<uint32_t> faces = PlanarShapeCellFaces(c.v);
	const std::vector<std::vector<PlanarCellWall>> cells = PlanarShapeCellWalls(c.v);
	REQUIRE(faces.size() == cells.size());
	REQUIRE(std::ranges::is_sorted(faces));
	std::vector<uint32_t> known;
	for (const PlanarFaceWalls &f : PlanarShapeFaceWalls(c.v)) known.push_back(f.face);
	for (const uint32_t f : faces) CHECK(std::ranges::find(known, f) != known.end());
}

TEST_CASE("planar draw: PSG 197 clearance per cell, faces alone", "[.][planarkeepapart]")
{
	const PlanarShapeVariant shaped = Shaped197();
	for (const uint32_t f : {0u, 50u, 99u, 120u, 55u, 77u, 92u, 143u}) {
		const PlanarDrawResult r = CheckPlanarDrawable(shaped, 30.0, {.only_faces = {f}});
		WARN(fmt::format("KA cells face {}: feasible {}, status {}, rows {}, {:.0f} ms", f, r.feasible, r.status, r.rows, r.ms));
		for (size_t i = 0; i < std::min<size_t>(10, r.conflict.size()); i++) WARN(fmt::format("KA cells face {} conflict {}: {}", f, i, r.conflict[i]));
		CHECK(r.feasible);
	}
	const PlanarDrawResult r4 = CheckPlanarDrawable(shaped, 30.0, {.only_faces = {4}});
	WARN(fmt::format("KA cells face 4: feasible {}, status {}, rows {}, {:.0f} ms", r4.feasible, r4.status, r4.rows, r4.ms));
	for (size_t i = 0; i < std::min<size_t>(10, r4.conflict.size()); i++) WARN(fmt::format("KA cells face 4 conflict {}: {}", i, r4.conflict[i]));
}

/** Warn one LD line for a solve, with its conflict families and the first 20 tags when infeasible. */
static void WarnLd(const std::string &label, const PlanarDrawResult &r, size_t changed, size_t kept, size_t nohit)
{
	WARN(fmt::format("{}: feasible {}, status {}, rows {}, columns {}, {:.0f} ms, changed {}, kept {}, nohit {}", label, r.feasible, r.status, r.rows, r.columns, r.ms, changed, kept, nohit));
	if (r.feasible) return;
	std::map<std::string, size_t> families;
	for (const std::string &tag : r.conflict) families[tag.substr(0, tag.find(' '))]++;
	for (const auto &[family, count] : families) WARN(fmt::format("{} conflicts: {} {}", label, family, count));
	for (size_t i = 0; i < std::min<size_t>(20, r.conflict.size()); i++) WARN(fmt::format("{} conflict {}: {}", label, i, r.conflict[i]));
}

TEST_CASE("planar draw: PSG 197 with landings from a drawing", "[.][planarkeepapart]")
{
	const PlanarShapeVariant shaped = Shaped197();
	const PlanarDrawResult base = CheckPlanarDrawable(shaped, 30.0, {.clearance = false});
	WARN(fmt::format("LD 197 base (no clearance): feasible {}, status {}, rows {}, {:.0f} ms", base.feasible, base.status, base.rows, base.ms));
	REQUIRE(base.feasible);
	int width = 0, height = 0;
	for (const auto &track : base.points) {
		for (const auto &p : track) {
			width = std::max(width, p[0]);
			height = std::max(height, p[1]);
		}
	}
	const std::vector<PlanarFaceWalls> faces = PlanarShapeFaceWalls(shaped);
	PlanarShapeVariant relanded = shaped;
	size_t changed = 0, kept = 0, nohit = 0;
	for (size_t di = 0; di < shaped.dummies.size(); di++) {
		const PlanarShapeDummy &d = shaped.dummies[di];
		const auto it = std::ranges::find_if(faces, [&](const PlanarFaceWalls &f) { return f.face == d.face; });
		REQUIRE(it != faces.end());
		const std::array<int, 2> p = base.points[d.track][d.piece + 1];
		const std::array<int, 2> s = PlanarModel::STEP[d.heading & 7];
		double best = 0;
		bool found = false;
		uint32_t hit_track = 0, hit_piece = 0;
		for (const PlanarCellWall &w : it->walls) {
			double t = -1;
			if (w.track == UINT32_MAX) {
				const PlanarSide side = static_cast<PlanarSide>(w.piece);
				const int axis = (side == PlanarSide::N || side == PlanarSide::S) ? 1 : 0;
				const int line = (side == PlanarSide::N || side == PlanarSide::W) ? 0 : (axis == 1 ? height : width);
				if (s[axis] == 0) continue;
				t = static_cast<double>(line - p[axis]) / s[axis];
			} else if (w.track == PLANAR_DUMMY_TRACK) {
				continue;
			} else {
				if (w.track == d.track && (w.piece == d.piece || w.piece == d.piece + 1)) continue;
				const std::array<int, 2> a = base.points[w.track][w.piece];
				const std::array<int, 2> b = base.points[w.track][w.piece + 1];
				const int ex = b[0] - a[0], ey = b[1] - a[1];
				if (ex == 0 && ey == 0) continue;
				const int det = s[0] * ey - s[1] * ex;
				if (det == 0) continue;
				const int ax = a[0] - p[0], ay = a[1] - p[1];
				t = static_cast<double>(ax * ey - ay * ex) / det;
				const double u = static_cast<double>(ax * s[1] - ay * s[0]) / det;
				if (u < -1e-9 || u > 1 + 1e-9) continue;
			}
			if (t <= 1e-9) continue;
			if (!found || t < best - 1e-9) {
				found = true;
				best = t;
				hit_track = w.track;
				hit_piece = w.piece;
			}
		}
		if (!found) {
			nohit++;
			continue;
		}
		if (hit_track == d.land_track && hit_piece == d.land_piece) {
			kept++;
		} else {
			changed++;
		}
		relanded.dummies[di].land_track = hit_track;
		relanded.dummies[di].land_piece = hit_piece;
	}
	const PlanarDrawResult r = CheckPlanarDrawable(relanded, 120.0);
	WarnLd("LD 197", r, changed, kept, nohit);
	const PlanarDrawResult r2 = CheckPlanarDrawable(relanded, 60.0, {.clearance = true, .clear_rows = false});
	WarnLd("LD 197 landings only", r2, changed, kept, nohit);
	WARN(fmt::format("LD 197 cells: relanded {}, shaped {}", PlanarShapeCellWalls(relanded).size(), PlanarShapeCellWalls(shaped).size()));
}
