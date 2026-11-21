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
#include <charconv>
#include <chrono>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
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

/** Read a file of the extractor's PSG 197 output (X8). */
static std::string ReadExtractorFileForDraw(const std::string &name)
{
	std::ifstream file(std::string(JUNCTION_TOPOLOGY_DIR) + "/../corpus/out/psg197/" + name);
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

/** The five X8 stages for one set of files; each WARNs its outcome and stops on failure. */
static void LoadStagesX8(const std::string &label, const std::string &topology_text, const std::string &structure_text, const std::string &shape_text)
{
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(topology_text, &error);
	if (!error.empty() || topology.lines.empty()) {
		WARN(fmt::format("X8 {} 1 parse: FAILED: {}", label, error));
		return;
	}
	WARN(fmt::format("X8 {} 1 parse: ports {}, lines {}, nodes {}, crossings {}", label, topology.ports.size(), topology.lines.size(), topology.nodes.size(), topology.crossings.size()));

	const JunctionSpec spec = MakeTestJunctionSpec();
	std::vector<std::string> report;
	const PlanarStructureVariant structure = LoadPlanarStructureVariant(topology, structure_text, spec, report, &error);
	if (!error.empty()) {
		WARN(fmt::format("X8 {} 2 structure: FAILED: {}", label, error));
		return;
	}
	size_t gaps = 0, differs = 0, other = 0;
	for (const std::string &r : report) {
		if (r.starts_with("gap:")) {
			gaps++;
		} else if (r.starts_with("differs:")) {
			differs++;
		} else {
			other++;
		}
	}
	WARN(fmt::format("X8 {} 2 structure: report lines {} (gap: {}, differs: {}, other {})", label, report.size(), gaps, differs, other));
	for (size_t i = 0; i < std::min<size_t>(20, report.size()); i++) WARN(fmt::format("X8 {} 2 report {}: {}", label, i, report[i]));

	PlanarShapeVariant shaped;
	std::string why;
	if (!LoadPlanarShapeVariant(topology, structure, shape_text, shaped, why)) {
		WARN(fmt::format("X8 {} 3 shape: FAILED: {}", label, why));
		return;
	}
	const std::vector<std::string> breaches = CheckPlanarShapePromise(topology, spec, shaped);
	WARN(fmt::format("X8 {} 3 shape: loaded, promise breaches {}", label, breaches.size()));
	for (size_t i = 0; i < std::min<size_t>(20, breaches.size()); i++) WARN(fmt::format("X8 {} 3 breach {}: {}", label, i, breaches[i]));

	if (!PlacePlanarShapeFlex(shaped, why)) {
		WARN(fmt::format("X8 {} 4 flex: FAILED: {}", label, why));
		return;
	}
	size_t flex = 0;
	for (const PlanarShapeTrack &t : shaped.tracks) flex += t.flex.size();
	std::string cells_why;
	const bool refined = RefinePlanarShapeCells(shaped, cells_why);
	WARN(fmt::format("X8 {} 4 flex {}, cells refined {}: {}, dummies {}, cells {}", label, flex, refined, cells_why, shaped.dummies.size(), PlanarShapeCellWalls(shaped).size()));
	if (!refined) return;

	const PlanarDrawResult r = CheckPlanarDrawable(shaped, 60.0, {.elastic = true});
	double family_sum = 0;
	std::map<std::string, std::pair<size_t, double>> families;
	for (const auto &[tag, slack] : r.stretched) {
		auto &f = families[tag.substr(0, tag.find(' '))];
		f.first++;
		f.second += slack;
		family_sum += slack;
	}
	WARN(fmt::format("X8 {} 5 draw: status {}, total slack {:.3f}, stretched {} (sum {:.3f}), {:.0f} ms", label, r.status, r.slack, r.stretched.size(), family_sum, r.ms));
	for (const auto &[family, f] : families) WARN(fmt::format("X8 {} 5 family {}: count {}, sum {:.3f}", label, family, f.first, f.second));
	for (size_t i = 0; i < std::min<size_t>(30, r.stretched.size()); i++) WARN(fmt::format("X8 {} 5 stretched {}: {} {:.3f}", label, i, r.stretched[i].first, r.stretched[i].second));
}

TEST_CASE("planar draw: the extractor's PSG 197 loads (X8)", "[.][planarpsg197]")
{
	try {
		LoadStagesX8("extractor", ReadExtractorFileForDraw("topology.txt"), ReadExtractorFileForDraw("structure.txt"), ReadExtractorFileForDraw("shape.txt"));
	} catch (const std::exception &e) {
		WARN(fmt::format("X8 extractor: EXCEPTION in the first stage not reported above: {}", e.what()));
	}
	LoadStagesX8("hand", ReadTopologyFileForDraw("psg197.txt"), ReadTopologyFileForDraw("psg197.structure.txt"), ReadTopologyFileForDraw("psg197.shape.txt"));
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

TEST_CASE("planar draw: pairwise sides from a drawing are plain rows", "[junction][planar][planardraw]")
{
	const DrawCase c = ShapeFirst(PlanarTText(1), 1);
	const PlanarDrawResult free = CheckPlanarDrawable(c.v, 0.1, {.pairwise = true});
	const PlanarDrawResult loose = CheckPlanarDrawable(c.v, 5.0, {.clearance = false});
	REQUIRE(loose.feasible);
	const PlanarDrawResult fixed = CheckPlanarDrawable(c.v, 5.0, {.pairwise = true, .sides_from = &loose.points});
	CHECK(fixed.binaries == 0);
	CHECK(fixed.pairs == free.pairs);
	/* A drawing without clearance keeps fewer sides. */
	CHECK(fixed.short_sides > 0);
	/* A reference that is not a drawing of this variant is refused. */
	std::vector<std::vector<std::array<int, 2>>> wrong = loose.points;
	wrong.front().pop_back();
	const PlanarDrawResult refused = CheckPlanarDrawable(c.v, 5.0, {.pairwise = true, .sides_from = &wrong});
	CHECK(refused.status.starts_with("unsupported"));
}

/** The port lines of a side in reading order with the index into their port's `lines`, as the drawing program reads them. */
static std::vector<std::pair<uint32_t, size_t>> SideReadingOrder(const PlanarTopology &t, PlanarSide side, uint32_t &first_port)
{
	std::vector<std::pair<uint32_t, size_t>> order; // (port, index into lines)
	first_port = UINT32_MAX;
	for (uint32_t p = 0; p < t.ports.size(); p++) {
		const PlanarPort &port = t.ports[p];
		if (port.side != side) continue;
		if (first_port == UINT32_MAX) first_port = p;
		const bool along_x = side == PlanarSide::N || side == PlanarSide::S;
		const int right = PlanarModel::STEP[(static_cast<int>(port.heading) + 2) % 8][along_x ? 0 : 1];
		for (size_t k = 0; k < port.lines.size(); k++) order.emplace_back(p, right < 0 ? port.lines.size() - 1 - k : k);
	}
	return order;
}

TEST_CASE("planar draw: port positions set the lane spacing", "[junction][planar][planardraw]")
{
	/* The positions are set on the drawing program's own copy, v.structure.map.expanded.ports. */
	DrawCase c = ShapeFirst(PlanarTText(1), 1);
	std::vector<PlanarPort> &ports = c.v.structure.map.expanded.ports;
	PlanarTopology holder;
	holder.ports = ports;
	for (const PlanarSide side : {PlanarSide::N, PlanarSide::E, PlanarSide::S, PlanarSide::W}) {
		uint32_t first;
		int next = 10;
		for (PlanarPort &port : ports) {
			if (port.side == side) port.at.assign(port.lines.size(), 0);
		}
		for (const auto &[p, index] : SideReadingOrder(holder, side, first)) {
			ports[p].at[index] = next;
			next += 3;
		}
	}
	const PlanarDrawResult r = CheckPlanarDrawable(c.v, 5.0, {.clearance = false});
	INFO("status " << r.status);
	REQUIRE(r.feasible);
	for (const PlanarSide side : {PlanarSide::N, PlanarSide::E, PlanarSide::S, PlanarSide::W}) {
		uint32_t first;
		const bool along_x = side == PlanarSide::N || side == PlanarSide::S;
		std::vector<int> got;
		for (const auto &[p, index] : SideReadingOrder(holder, side, first)) {
			const uint32_t line = ports[p].lines[index];
			const std::vector<std::array<int, 2>> &points = r.points[line];
			got.push_back((ports[p].in ? points.front() : points.back())[along_x ? 0 : 1]);
		}
		for (size_t i = 1; i < got.size(); i++) CHECK(got[i] - got[i - 1] == 6);
	}

	/* Out of reading order is refused. */
	DrawCase d = ShapeFirst(PlanarTText(1), 1);
	for (PlanarPort &port : d.v.structure.map.expanded.ports) {
		if (port.side == PlanarSide::W) port.at.assign(port.lines.size(), 10);
	}
	const PlanarDrawResult u = CheckPlanarDrawable(d.v, 5.0, {.clearance = false});
	CHECK(u.status.find("port positions") != std::string::npos);
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

TEST_CASE("planar draw: PSG 197 zero-length steps, faces and whole", "[.][planarkeepapart]")
{
	const PlanarShapeVariant shaped = Shaped197();
	const auto begin = std::chrono::steady_clock::now();
	size_t feasible = 0, infeasible = 0, undecided = 0, skipped = 0;
	std::vector<uint32_t> bad;
	for (const PlanarFaceWalls &f : PlanarShapeFaceWalls(shaped)) {
		if (std::chrono::steady_clock::now() - begin > std::chrono::minutes(15)) {
			skipped++;
			continue;
		}
		const PlanarDrawResult r = CheckPlanarDrawable(shaped, 15.0, {.only_faces = {f.face}, .tidy = false});
		if (r.feasible) {
			feasible++;
			continue;
		}
		(r.status == "Infeasible" ? infeasible : undecided)++;
		bad.push_back(f.face);
		WARN(fmt::format("FSf face {}: status {}, {:.0f} ms", f.face, r.status, r.ms));
	}
	if (skipped > 0) WARN(fmt::format("FSf stopped after 15 min: {} faces skipped", skipped));
	const PlanarDrawResult whole = CheckPlanarDrawable(shaped, 60.0, {.tidy = false});
	WARN(fmt::format("FSf 197 faces: feasible {}, infeasible {}, undecided {}, skipped {}; whole: {} in {:.0f} ms", feasible, infeasible, undecided, skipped, whole.status, whole.ms));
	for (const uint32_t face : bad) {
		const PlanarDrawResult e = CheckPlanarDrawable(shaped, 15.0, {.only_faces = {face}, .elastic = true});
		WARN(fmt::format("FSf elastic face {}: status {}, total slack {}", face, e.status, e.slack));
		for (size_t i = 0; i < std::min<size_t>(15, e.stretched.size()); i++) WARN(fmt::format("FSf elastic face {} stretched {}: {} {}", face, i, e.stretched[i].first, e.stretched[i].second));
	}
	if (!whole.feasible) {
		const PlanarDrawResult e = CheckPlanarDrawable(shaped, 60.0, {.elastic = true});
		WARN(fmt::format("FSf elastic whole: status {}, total slack {}", e.status, e.slack));
		std::map<std::string, std::pair<size_t, double>> families;
		for (const auto &[tag, amount] : e.stretched) {
			auto &f = families[tag.substr(0, tag.find(' '))];
			f.first++;
			f.second += amount;
		}
		for (const auto &[family, cs] : families) WARN(fmt::format("FSf elastic whole family {}: {} rows, slack {}", family, cs.first, cs.second));
		for (size_t i = 0; i < std::min<size_t>(40, e.stretched.size()); i++) WARN(fmt::format("FSf elastic whole stretched {}: {} {}", i, e.stretched[i].first, e.stretched[i].second));
	}
}

TEST_CASE("planar draw: PSG 197 with V's port positions, faces and whole", "[.][planarkeepapart]")
{
	const PlanarShapeVariant shaped = Shaped197();
	const auto begin = std::chrono::steady_clock::now();
	size_t feasible = 0, infeasible = 0, undecided = 0, skipped = 0;
	std::vector<uint32_t> bad;
	for (const PlanarFaceWalls &f : PlanarShapeFaceWalls(shaped)) {
		if (std::chrono::steady_clock::now() - begin > std::chrono::minutes(15)) {
			skipped++;
			continue;
		}
		const PlanarDrawResult r = CheckPlanarDrawable(shaped, 15.0, {.only_faces = {f.face}, .tidy = false});
		if (r.feasible) {
			feasible++;
			continue;
		}
		(r.status == "Infeasible" ? infeasible : undecided)++;
		bad.push_back(f.face);
		WARN(fmt::format("FSg face {}: status {}, {:.0f} ms", f.face, r.status, r.ms));
	}
	if (skipped > 0) WARN(fmt::format("FSg stopped after 15 min: {} faces skipped", skipped));
	const PlanarDrawResult whole = CheckPlanarDrawable(shaped, 60.0, {.tidy = false});
	WARN(fmt::format("FSg 197 faces: feasible {}, infeasible {}, undecided {}, skipped {}; whole: {} in {:.0f} ms", feasible, infeasible, undecided, skipped, whole.status, whole.ms));
	for (const uint32_t face : bad) {
		const PlanarDrawResult e = CheckPlanarDrawable(shaped, 15.0, {.only_faces = {face}, .elastic = true});
		WARN(fmt::format("FSg elastic face {}: status {}, total slack {}", face, e.status, e.slack));
		for (size_t i = 0; i < std::min<size_t>(15, e.stretched.size()); i++) WARN(fmt::format("FSg elastic face {} stretched {}: {} {}", face, i, e.stretched[i].first, e.stretched[i].second));
	}
	if (!whole.feasible) {
		const PlanarDrawResult e = CheckPlanarDrawable(shaped, 60.0, {.elastic = true});
		WARN(fmt::format("FSg elastic whole: status {}, total slack {}", e.status, e.slack));
		std::map<std::string, std::pair<size_t, double>> families;
		for (const auto &[tag, amount] : e.stretched) {
			auto &f = families[tag.substr(0, tag.find(' '))];
			f.first++;
			f.second += amount;
		}
		for (const auto &[family, cs] : families) WARN(fmt::format("FSg elastic whole family {}: {} rows, slack {}", family, cs.first, cs.second));
		for (size_t i = 0; i < std::min<size_t>(40, e.stretched.size()); i++) WARN(fmt::format("FSg elastic whole stretched {}: {} {}", i, e.stretched[i].first, e.stretched[i].second));
	}
}

/** The hand cell of the zero-length step test: an insert between two corners, a closing wall each side. */
static const std::vector<PlanarCellWall> ZERO_CELL = {{0, 0, 6}, {1, 0, 3}, {1, 1, 2}, {1, 2, 3}, {2, 0, 2}, {UINT32_MAX, 1, 0}};
static const std::vector<bool> ZERO_INSERT = {false, false, true, false, false, false};

TEST_CASE("planar draw: a step of length 0 imposes no spacing", "[junction][planar][planardraw]")
{
	constexpr uint32_t N = UINT32_MAX;
	using Row = std::tuple<uint32_t, uint32_t, int, uint32_t>;
	std::vector<Row> got;
	const std::vector<PlanarClearRow> rows = PlanarCellClearRows(ZERO_CELL, ZERO_INSERT);
	for (const PlanarClearRow &r : rows) got.emplace_back(r.vertex, r.wall, r.spacing, r.room);
	std::ranges::sort(got);
	const std::vector<Row> want = {{0, 4, 2, N}, {1, 0, 1, 2}, {1, 4, 1, 2}, {2, 0, 1, 2}, {2, 4, 1, 2}, {3, 0, 2, N}, {4, 0, 2, N}, {4, 1, 2, N}, {4, 3, 2, N}, {5, 1, 2, N}, {5, 3, 2, N}, {5, 4, 2, N}};
	CHECK(got == want);
	constexpr int M = 4096;
	for (const int L : {0, 2}) {
		const std::array<std::array<int, 2>, 6> V = {{{10, 10}, {11, 11}, {11 + L, 11}, {12 + L, 12}, {20, 12}, {20, 10}}};
		std::vector<std::pair<uint32_t, uint32_t>> room_fails;
		for (const PlanarClearRow &r : rows) {
			const std::array<int, 2> from = V[(r.wall + 5) % 6];
			const std::array<int, 2> normal = PlanarModel::STEP[(ZERO_CELL[r.wall].heading + 6) % 8];
			const int d = (V[r.vertex][0] - from[0]) * normal[0] + (V[r.vertex][1] - from[1]) * normal[1];
			INFO("L " << L << " row " << r.vertex << " " << r.wall);
			CHECK(d >= r.spacing);
			if (r.room != N && M * (d - r.spacing) - L < 0) room_fails.emplace_back(r.vertex, r.wall);
		}
		std::ranges::sort(room_fails);
		if (L == 0) CHECK(room_fails.empty());
		else CHECK(room_fails == std::vector<std::pair<uint32_t, uint32_t>>{{1, 0}, {1, 4}, {2, 0}, {2, 4}});
	}
}

TEST_CASE("planar draw: elastic mode names the stretched rows", "[junction][planar][planardraw]")
{
	DrawCase c = ShapeFirst(PlanarTText(1), 1);
	c.v.structure.side_gap = {0, 0, 0, 0};
	const PlanarDrawResult r = CheckPlanarDrawable(c.v, 5.0, {.elastic = true});
	std::string stretched;
	for (const auto &[tag, amount] : r.stretched) stretched += fmt::format("{} {}; ", tag, amount);
	INFO("status " << r.status << " slack " << r.slack << " stretched " << stretched);
	CHECK(r.feasible);
	CHECK(r.slack > 0);
	CHECK(std::ranges::any_of(r.stretched, [](const auto &s) { return s.first.starts_with("clear ") || s.first.starts_with("port ") || s.first.starts_with("box"); }));

	const DrawCase t1 = ShapeFirst(PlanarTText(1), 1);
	const PlanarDrawResult ok = CheckPlanarDrawable(t1.v, 5.0, {.clearance = false, .elastic = true});
	CHECK(ok.feasible);
	CHECK(std::abs(ok.slack) < 1e-6);
	CHECK(ok.stretched.empty());
}

/** The pin text of one point per entry: `end <line> start|end X Y`. */
static std::string EndPinLine(const PlanarTrackMap &map, uint32_t track, bool start, std::array<int, 2> at)
{
	return fmt::format("end {} {} {} {}\n", map.tracks[track].name, start ? "start" : "end", at[0], at[1]);
}

TEST_CASE("planar draw: a crosser may cross at its piece end (D414)", "[junction][planar][planardraw]")
{
	const DrawCase c = ShapeFirst(PlanarTText(1), 1);
	const PlanarTrackMap &map = c.v.structure.map;
	const PlanarDrawResult base = CheckPlanarDrawable(c.v, 5.0, {.clearance = false});
	REQUIRE(base.feasible);

	/* A crossing of a structure, its crosser's piece start. */
	uint32_t crossing = UINT32_MAX;
	for (const PlanarStructure &structure : c.v.structure.structures) {
		if (!structure.crossings.empty()) crossing = structure.crossings.front();
		if (crossing != UINT32_MAX) {
			const uint32_t separator = structure.separator;
			const uint32_t crosser = map.crossings[crossing].a == separator ? map.crossings[crossing].b : map.crossings[crossing].a;
			const auto at = std::ranges::find(map.events[crosser], crossing);
			REQUIRE(at != map.events[crosser].end());
			const uint32_t piece = c.v.tracks[crosser].event_piece[at - map.events[crosser].begin()];
			const std::array<int, 2> start = base.points[crosser][piece];
			/* The answer key's end points hold the drawing in place, so the crossing pin is no free shift. */
			std::string text = fmt::format("cross {} {} {} {}\n", map.tracks[map.crossings[crossing].a].name, map.tracks[map.crossings[crossing].b].name, start[0], start[1]);
			for (const uint32_t t : {map.crossings[crossing].a, map.crossings[crossing].b}) {
				text += EndPinLine(map, t, true, base.points[t].front());
				text += EndPinLine(map, t, false, base.points[t].back());
			}
			PlanarDrawPins pins = ParsePlanarDrawPins(c.v, text);
			pins.window = 0;
			REQUIRE(pins.unmatched.empty());
			const PlanarDrawResult held = CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .pins = &pins});
			CHECK(held.feasible);
			return;
		}
	}
	FAIL("T1 has no structured crossing");
}

TEST_CASE("planar draw: pins hold points to an answer key", "[junction][planar][planardraw]")
{
	const DrawCase c = ShapeFirst(PlanarTText(1), 1);
	const PlanarTrackMap &map = c.v.structure.map;
	const PlanarDrawResult base = CheckPlanarDrawable(c.v, 5.0, {.clearance = false});
	REQUIRE(base.feasible);

	std::string text = "# an answer key, shifted\n# half tiles\n";
	size_t named = 0; // Copies and rows may share a line's name; only the track that carries it is pinned.
	for (uint32_t t = 0; t < map.tracks.size(); t++) {
		if (std::ranges::count(map.tracks, map.tracks[t].name, &PlanarTrack::name) != 1) continue;
		named++;
		const std::array<int, 2> a = base.points[t].front(), b = base.points[t].back();
		text += EndPinLine(map, t, true, {a[0] + 100, a[1] + 60});
		text += EndPinLine(map, t, false, {b[0] + 100, b[1] + 60});
	}
	const PlanarDrawPins pins = ParsePlanarDrawPins(c.v, text);
	CHECK(pins.unmatched.empty());
	REQUIRE(named > 0);
	REQUIRE(pins.pins.size() == 2 * named);

	const PlanarDrawResult held = CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .pins = &pins});
	
	REQUIRE(held.feasible);
	std::array<int, 2> lo = {1 << 20, 1 << 20}, hi = {-(1 << 20), -(1 << 20)};
	for (const PlanarDrawPins::Pin &pin : pins.pins) {
		const std::array<int, 2> p = pin.start ? held.points[pin.track].front() : held.points[pin.track].back();
		for (int axis = 0; axis < 2; axis++) {
			lo[axis] = std::min(lo[axis], p[axis] - pin.at[axis]);
			hi[axis] = std::max(hi[axis], p[axis] - pin.at[axis]);
		}
	}
	INFO("the shifts found span x " << lo[0] << ".." << hi[0] << ", y " << lo[1] << ".." << hi[1]);
	CHECK(hi[0] - lo[0] <= 2 * pins.window);
	CHECK(hi[1] - lo[1] <= 2 * pins.window);

	/* One pin moved 40 half tiles along x cannot be held. */
	PlanarDrawPins moved = pins;
	moved.pins[0].at[0] += 40;
	const PlanarDrawResult broken = CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .pins = &moved});
	CHECK(!broken.feasible);
	CHECK(HasFamily(broken, "pin "));
	/* In elastic mode a pin that a node row can give way to leaves the pin rows hard and stretches another family. */
	size_t elastic_ok = 0;
	for (size_t k = 0; k < pins.pins.size(); k++) {
		PlanarDrawPins other = pins;
		other.pins[k].at[0] += 40;
		const PlanarDrawResult stretched = CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .elastic = true, .pins = &other});
		if (!stretched.feasible) continue;
		elastic_ok++;
		CHECK(!std::ranges::any_of(stretched.stretched, [](const auto &s) { return s.first.starts_with("pin "); }));
		CHECK(!stretched.stretched.empty());
	}
	INFO("elastic solves feasible with a moved pin: " << elastic_ok);
	CHECK(elastic_ok > 0);

	/* A line the map does not have goes to unmatched. */
	const PlanarDrawPins unknown = ParsePlanarDrawPins(c.v, "end NOPE start 1 2\ncross L1 NOPE 3 4\n" + EndPinLine(map, 0, true, {1, 2}));
	CHECK(unknown.unmatched.size() == 2);
	CHECK(unknown.pins.size() == 1);
}

TEST_CASE("planar draw: a port lane may be 0 long before its first node (D412)", "[junction][planar][planardraw]")
{
	size_t found = 0;
	for (const size_t lanes : {1}) {
		const DrawCase c = ShapeFirst(PlanarTText(lanes), 1);
		const PlanarTrackMap &map = c.v.structure.map;
		const PlanarDrawResult base = CheckPlanarDrawable(c.v, 5.0, {.clearance = false});
		REQUIRE(base.feasible);
		for (uint32_t t = 0; t < map.tracks.size(); t++) {
			const PlanarEnd &from = map.expanded.lines[t].from, &to = map.expanded.lines[t].to;
			if (from.port == to.port || !map.events[t].empty()) continue;
			const std::array<int, 2> a = base.points[t].front(), b = base.points[t].back();
			if (a == b) continue;
			/* Pin the line's first point and pin its last point 4 behind it along the line's direction: the line is then 0 long, its node on the box edge. */
			std::array<int, 2> d{};
			for (int axis = 0; axis < 2; axis++) d[axis] = (b[axis] > a[axis]) - (b[axis] < a[axis]);
			const std::string text = EndPinLine(map, t, true, a) + EndPinLine(map, t, false, {a[0] - 4 * d[0], a[1] - 4 * d[1]});
			const PlanarDrawPins pins = ParsePlanarDrawPins(c.v, text);
			REQUIRE(pins.pins.size() == 2);
			const PlanarDrawResult r = CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .pins = &pins});
			/* A node on the box edge for every lane at once need not be drawable (its sibling lane's port is another point of the edge), so count the lines that are. */
			if (r.feasible) found++;
		}
	}
	INFO("lines drawable with their node on the box edge: " << found);
	CHECK(found > 0);
}

/** The extractor's PSG 197, shaped and flexed, and V's pins; false when pins.txt is missing. */
static bool LoadPinned197(PlanarShapeVariant &shaped, PlanarDrawPins &pins)
{
	std::ifstream file(std::string(JUNCTION_TOPOLOGY_DIR) + "/../corpus/out/psg197/pins.txt");
	if (!file) {
		WARN("no pins yet");
		return false;
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(ReadExtractorFileForDraw("topology.txt"), &error);
	REQUIRE(error.empty());
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::vector<std::string> report;
	const PlanarStructureVariant structure = LoadPlanarStructureVariant(topology, ReadExtractorFileForDraw("structure.txt"), spec, report, &error);
	REQUIRE(error.empty());
	std::string why;
	REQUIRE(LoadPlanarShapeVariant(topology, structure, ReadExtractorFileForDraw("shape.txt"), shaped, why));
	REQUIRE(PlacePlanarShapeFlex(shaped, why));
	REQUIRE(RefinePlanarShapeCells(shaped, why));
	pins = ParsePlanarDrawPins(shaped, buffer.str());
	return true;
}

TEST_CASE("planar draw: the extractor's PSG 197 with pins", "[.][planarkeepapart]")
{
	PlanarShapeVariant shaped;
	PlanarDrawPins pins;
	if (!LoadPinned197(shaped, pins)) return;
	WARN(fmt::format("pins {}, unmatched {}", pins.pins.size(), pins.unmatched.size()));
	for (size_t i = 0; i < std::min<size_t>(10, pins.unmatched.size()); i++) WARN(fmt::format("unmatched {}: {}", i, pins.unmatched[i]));
	const PlanarDrawResult r = CheckPlanarDrawable(shaped, 60.0, {.elastic = true, .pins = &pins});
	std::map<std::string, std::pair<size_t, double>> families;
	for (const auto &[tag, slack] : r.stretched) {
		auto &f = families[tag.substr(0, tag.find(' '))];
		f.first++;
		f.second += slack;
	}
	WARN(fmt::format("pins 197: status {}, total slack {:.3f}, stretched {}, {:.0f} ms", r.status, r.slack, r.stretched.size(), r.ms));
	for (const auto &[family, f] : families) WARN(fmt::format("pins 197 family {}: count {}, sum {:.3f}", family, f.first, f.second));
	for (size_t i = 0; i < std::min<size_t>(40, r.stretched.size()); i++) WARN(fmt::format("pins 197 stretched {}: {} {:.3f}", i, r.stretched[i].first, r.stretched[i].second));
}

TEST_CASE("planar draw: PSG 197 integer base with pins", "[.][planarkeepapart]")
{
	PlanarShapeVariant shaped;
	PlanarDrawPins pins;
	if (!LoadPinned197(shaped, pins)) return;
	for (const int window : {2, 8}) {
		PlanarDrawPins pw = pins;
		pw.window = window;
		const PlanarDrawResult r = CheckPlanarDrawable(shaped, 60.0, {.clearance = false, .pins = &pw});
		WARN(fmt::format("integer base W{}: status {}, {:.0f} ms", window, r.status, r.ms));
		for (const std::string &tag : r.conflict) WARN(fmt::format("integer base W{} conflict: {}", window, tag));
	}
}

TEST_CASE("planar draw: PSG 197 integer base, minimal pin core", "[.][planarkeepapart]")
{
	PlanarShapeVariant shaped;
	PlanarDrawPins pins;
	if (!LoadPinned197(shaped, pins)) return;
	const PlanarTrackMap &map = shaped.structure.map;
	const auto solve = [&](const PlanarDrawPins &p) { return CheckPlanarDrawable(shaped, 10.0, {.clearance = false, .pins = &p}); };
	PlanarDrawPins none = pins;
	none.pins.clear();
	const PlanarDrawResult a = solve(none);
	WARN(fmt::format("core step A (no pins): status {}, {:.0f} ms", a.status, a.ms));
	for (const int window : {2, 8}) {
		PlanarDrawPins pw = pins;
		pw.window = window;
		const PlanarDrawResult b = solve(pw);
		WARN(fmt::format("core step B W{}: status {}, {:.0f} ms, {} pins", window, b.status, b.ms, pw.pins.size()));
		if (b.status != "Infeasible") {
			WARN(fmt::format("core W{}: not Infeasible, filter skipped", window));
			continue;
		}
		PlanarDrawPins core = pw;
		uint32_t solves = 0;
		double total_ms = 0;
		for (size_t i = 0; i < pw.pins.size(); i++) {
			PlanarDrawPins trial = core;
			const auto it = std::ranges::find_if(trial.pins, [&](const PlanarDrawPins::Pin &q) {
				return q.track == pw.pins[i].track && q.cross == pw.pins[i].cross && q.start == pw.pins[i].start && q.at == pw.pins[i].at;
			});
			if (it == trial.pins.end()) continue;
			trial.pins.erase(it);
			const PlanarDrawResult r = solve(trial);
			solves++;
			total_ms += r.ms;
			if (r.status == "Infeasible") core = trial;
		}
		WARN(fmt::format("core W{}: {} solves, {:.0f} ms, core {} pins", window, solves, total_ms, core.pins.size()));
		for (const PlanarDrawPins::Pin &pin : core.pins) {
			if (pin.cross) {
				WARN(fmt::format("core W{} pin: cross {} {} at {} {}", window, map.tracks[map.crossings[pin.track].a].name, map.tracks[map.crossings[pin.track].b].name, pin.at[0], pin.at[1]));
			} else {
				WARN(fmt::format("core W{} pin: end {} {} at {} {}", window, map.tracks[pin.track].name, pin.start ? "start" : "end", pin.at[0], pin.at[1]));
			}
		}
		const PlanarDrawResult c = solve(core);
		WARN(fmt::format("core W{} re-solve: status {}", window, c.status));
		for (const std::string &tag : c.conflict) WARN(fmt::format("core W{} conflict: {}", window, tag));
	}
}

TEST_CASE("planar draw: PSG 197 integer base, which integrality", "[.][planarkeepapart]")
{
	PlanarShapeVariant shaped;
	PlanarDrawPins pins;
	if (!LoadPinned197(shaped, pins)) return;
	pins.window = 8;
	const std::vector<std::string> groups = {"point", "axis", "diagonal", "param", "cross", "other"};
	std::vector<std::vector<std::string>> trials = {{}};
	for (const std::string &g : groups) trials.push_back({g});
	for (const std::string &g : groups) {
		std::vector<std::string> rest;
		for (const std::string &o : groups) {
			if (o != g) rest.push_back(o);
		}
		trials.push_back(rest);
	}
	for (const std::vector<std::string> &x : trials) {
		const PlanarDrawResult r = CheckPlanarDrawable(shaped, 30.0, {.clearance = false, .pins = &pins, .continuous = x});
		std::string list;
		for (const std::string &g : x) list += (list.empty() ? "" : ",") + g;
		WARN(fmt::format("continuous {{{}}}: status {}, {:.0f} ms", list, r.status, r.ms));
	}
}

/** WARN the `limit` largest stretched `sep` rows of an elastic pairwise drawing, with the row, its slack and both walls. */
static void WarnSepWalls(const PlanarShapeVariant &shaped, const PlanarDrawResult &r, const std::string &label, size_t limit)
{
	const PlanarTrackMap &map = shaped.structure.map;
	const std::vector<PlanarFaceWalls> faces = PlanarShapeFaceWalls(shaped);
	size_t shown = 0;
	for (size_t i = 0; i < r.stretched.size() && shown < limit; i++) {
		const std::string &row = r.stretched[i].first;
		if (!row.starts_with("sep ")) continue;
		shown++;
		std::istringstream in(row.substr(4));
		uint32_t face = 0;
		size_t a = 0, b = 0;
		in >> face >> a >> b;
		std::string text;
		for (const PlanarFaceWalls &f : faces) {
			if (f.face != face || a >= f.walls.size() || b >= f.walls.size()) continue;
			for (const size_t k : {a, b}) {
				const PlanarCellWall &w = f.walls[k];
				text += fmt::format(" [{} piece {} heading {}]", map.tracks[w.track].name, w.piece, w.heading);
			}
		}
		WARN(fmt::format("{} sep row {} ({:.3f}):{}", label, row, r.stretched[i].second, text));
	}
}

TEST_CASE("planar draw: PSG 197 pairwise, sides from the pinned drawing", "[.][planarkeepapart]")
{
	PlanarShapeVariant shaped;
	PlanarDrawPins pins;
	if (!LoadPinned197(shaped, pins)) return;
	PlanarDrawPins p8 = pins;
	p8.window = 8;
	PlanarDrawPins p2 = pins;
	p2.window = 2;
	const PlanarDrawResult cells = CheckPlanarDrawable(shaped, 60.0, {.elastic = true, .pins = &p8});
	const PlanarDrawResult base = CheckPlanarDrawable(shaped, 60.0, {.clearance = false, .elastic = true, .pins = &p8});
	WARN(fmt::format("reference cells: {}, slack {:.3f}, {:.0f} ms; base: {}, slack {:.3f}, {:.0f} ms", cells.status, cells.slack, cells.ms, base.status, base.slack, base.ms));
	const std::array<std::pair<const char *, const PlanarDrawResult *>, 2> refs = {{{"cells", &cells}, {"base", &base}}};
	for (const auto &[name, ref] : refs) {
		if (!ref->feasible) continue;
		for (const PlanarDrawPins *pw : {&p2, &p8}) {
			const PlanarDrawResult r = CheckPlanarDrawable(shaped, 60.0, {.clearance = true, .clear_rows = true, .pairwise = true, .elastic = true, .pins = pw, .sides_from = &ref->points});
			const std::string tag = fmt::format("sides {} W{}", name, pw->window);
			WARN(fmt::format("{}: status {}, total slack {:.3f}, pairs {}, binaries {}, short_sides {}, {:.0f} ms", tag, r.status, r.slack, r.pairs, r.binaries, r.short_sides, r.ms));
			std::map<std::string, std::pair<size_t, double>> families;
			for (const auto &[row, slack] : r.stretched) {
				auto &f = families[row.substr(0, row.find(' '))];
				f.first++;
				f.second += slack;
			}
			for (const auto &[family, f] : families) WARN(fmt::format("{} family {}: count {}, sum {:.3f}", tag, family, f.first, f.second));
			for (size_t i = 0; i < std::min<size_t>(40, r.stretched.size()); i++) WARN(fmt::format("{} stretched {}: {} {:.3f}", tag, i, r.stretched[i].first, r.stretched[i].second));
			WarnSepWalls(shaped, r, tag, 20);
		}
	}
}

TEST_CASE("planar draw: PSG 197 sep-only slack, sides from the pinned drawing", "[.][planarkeepapart]")
{
	PlanarShapeVariant shaped;
	PlanarDrawPins pins;
	if (!LoadPinned197(shaped, pins)) return;
	PlanarDrawPins p8 = pins;
	p8.window = 8;
	PlanarDrawPins p4 = pins;
	p4.window = 4;
	const PlanarTrackMap &map = shaped.structure.map;
	const std::vector<PlanarFaceWalls> faces = PlanarShapeFaceWalls(shaped);
	const PlanarDrawResult ref0 = CheckPlanarDrawable(shaped, 60.0, {.elastic = true, .pins = &p8});
	WARN(fmt::format("reference 0 (cells, W8): {}, slack {:.3f}, {:.0f} ms", ref0.status, ref0.slack, ref0.ms));
	if (!ref0.feasible) return;
	for (const PlanarDrawPins *pw : {&p8, &p4}) {
		const std::vector<std::vector<std::array<int, 2>>> *sides = &ref0.points;
		PlanarDrawResult run;
		for (int n = 1; n <= 2; n++) {
			run = CheckPlanarDrawable(shaped, 60.0, {.clearance = true, .clear_rows = true, .pairwise = true, .elastic = true, .elastic_families = {"sep"}, .pins = pw, .sides_from = sides});
			const std::string tag = fmt::format("sep-only W{} run {}", pw->window, n);
			WARN(fmt::format("{}: status {}, total slack {:.3f}, stretched rows {}, short_sides {}, {:.0f} ms", tag, run.status, run.slack, run.stretched.size(), run.short_sides, run.ms));
			if (run.status != "Optimal") {
				std::string conflicts;
				for (const std::string &c : run.conflict) conflicts += c + "; ";
				WARN(fmt::format("{} conflict: {}", tag, conflicts));
			}
			WarnSepWalls(shaped, run, tag, 40);
			std::map<std::pair<std::string, std::string>, std::pair<size_t, double>> groups;
			for (const auto &[row, slack] : run.stretched) {
				if (!row.starts_with("sep ")) continue;
				std::istringstream in(row.substr(4));
				uint32_t face = 0;
				size_t a = 0, b = 0;
				in >> face >> a >> b;
				for (const PlanarFaceWalls &f : faces) {
					if (f.face != face || a >= f.walls.size() || b >= f.walls.size()) continue;
					auto &g = groups[{map.tracks[f.walls[a].track].name, map.tracks[f.walls[b].track].name}];
					g.first++;
					g.second += slack;
				}
			}
			std::vector<std::pair<std::pair<std::string, std::string>, std::pair<size_t, double>>> sorted(groups.begin(), groups.end());
			std::ranges::stable_sort(sorted, [](const auto &x, const auto &y) { return x.second.second > y.second.second; });
			for (size_t i = 0; i < std::min<size_t>(30, sorted.size()); i++) {
				WARN(fmt::format("{} tracks {} / {}: count {}, sum {:.3f}", tag, sorted[i].first.first, sorted[i].first.second, sorted[i].second.first, sorted[i].second.second));
			}
			if (!run.feasible) break;
			sides = &run.points;
		}
	}
}

TEST_CASE("planar draw: PSG 197 elastic, whole", "[.][planarkeepapart]")
{
	const PlanarShapeVariant shaped = Shaped197();
	const PlanarDrawResult r = CheckPlanarDrawable(shaped, 120.0, {.elastic = true});
	WARN(fmt::format("FSE status {}, {:.0f} ms, total slack {}, stretched rows {}", r.status, r.ms, r.slack, r.stretched.size()));
	std::map<std::string, std::pair<size_t, double>> families;
	std::map<uint32_t, std::pair<size_t, double>> faces;
	const std::vector<uint32_t> cell_face = PlanarShapeCellFaces(shaped);
	for (const auto &[tag, amount] : r.stretched) {
		auto &f = families[tag.substr(0, tag.find(' '))];
		f.first++;
		f.second += amount;
		if (tag.starts_with("clear ") || tag.starts_with("along ")) {
			const size_t a = tag.find(' ') + 1;
			size_t cell = 0;
			std::from_chars(tag.data() + a, tag.data() + tag.size(), cell);
			if (cell < cell_face.size()) {
				auto &g = faces[cell_face[cell]];
				g.first++;
				g.second += amount;
			}
		}
	}
	std::vector<std::pair<std::string, std::pair<size_t, double>>> fs(families.begin(), families.end());
	std::ranges::sort(fs, [](const auto &a, const auto &b) { return a.second.second > b.second.second; });
	for (const auto &[family, cs] : fs) WARN(fmt::format("FSE family {}: {} rows, slack {}", family, cs.first, cs.second));
	for (size_t i = 0; i < std::min<size_t>(40, r.stretched.size()); i++) WARN(fmt::format("FSE stretched {}: {} {}", i, r.stretched[i].first, r.stretched[i].second));
	std::vector<std::pair<uint32_t, std::pair<size_t, double>>> fc(faces.begin(), faces.end());
	std::ranges::sort(fc, [](const auto &a, const auto &b) { return a.second.second > b.second.second; });
	for (size_t i = 0; i < std::min<size_t>(30, fc.size()); i++) WARN(fmt::format("FSE face {}: {} rows, slack {}", fc[i].first, fc[i].second.first, fc[i].second.second));
}

TEST_CASE("planar draw: PSG 197 pocket rule, faces and whole", "[.][planarkeepapart]")
{
	const PlanarShapeVariant shaped = Shaped197();
	const auto begin = std::chrono::steady_clock::now();
	const std::set<uint32_t> before = {4, 6, 7, 8, 11, 12, 13, 14, 15, 16, 17, 46, 52, 64, 71, 72, 75, 84, 99, 124, 141, 147, 153};
	const std::set<uint32_t> tidy_off_feasible = {4, 6, 46, 52, 64, 72, 75, 84, 99, 124, 147, 153};
	size_t feasible = 0, infeasible = 0, undecided = 0, skipped = 0;
	for (const PlanarFaceWalls &f : PlanarShapeFaceWalls(shaped)) {
		if (std::chrono::steady_clock::now() - begin > std::chrono::minutes(15)) {
			skipped++;
			continue;
		}
		const PlanarDrawResult r = CheckPlanarDrawable(shaped, 15.0, {.only_faces = {f.face}, .pocket = true, .tidy = false});
		if (r.feasible) {
			feasible++;
			continue;
		}
		(r.status == "Infeasible" ? infeasible : undecided)++;
		std::string tags;
		for (size_t i = 0; i < std::min<size_t>(5, r.conflict.size()); i++) tags += " [" + r.conflict[i] + "]";
		WARN(fmt::format("FSd face {}: status {}, {:.0f} ms, conflict{}", f.face, r.status, r.ms, tags));
		if (!before.contains(f.face) || tidy_off_feasible.contains(f.face)) WARN(fmt::format("FSd face {} was feasible in the FS-b run and is not now", f.face));
	}
	if (skipped > 0) WARN(fmt::format("FSd stopped after 15 min: {} faces skipped", skipped));
	const PlanarDrawResult whole = CheckPlanarDrawable(shaped, 60.0, {.pocket = true, .tidy = false});
	WARN(fmt::format("FSd whole: status {}, rows {}, {:.0f} ms", whole.status, whole.rows, whole.ms));
	std::map<std::string, size_t> families;
	for (const std::string &tag : whole.conflict) families[tag.substr(0, tag.find(' '))]++;
	for (const auto &[family, count] : families) WARN(fmt::format("FSd whole conflicts: {} {}", family, count));
	for (size_t i = 0; i < std::min<size_t>(20, whole.conflict.size()); i++) WARN(fmt::format("FSd whole conflict {}: {}", i, whole.conflict[i]));
	WARN(fmt::format("FSd 197 faces: feasible {}, infeasible {}, undecided {}, skipped {}; whole: {} in {:.0f} ms", feasible, infeasible, undecided, skipped, whole.status, whole.ms));
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

/** The hand cell of the pocket tests: a step a, insert, b on the north side, two top walls, then left and bottom walls. */
static const std::vector<PlanarCellWall> POCKET_CELL = {{0, 0, 0}, {0, 1, 1}, {0, 2, 0}, {1, 0, 5}, {4, 0, 5}, {2, 0, 4}, {3, 0, 2}};
static const std::vector<bool> POCKET_INSERT = {false, true, false, false, false, false, false};

/** The rows of the pocket cell that fail on the vertex points: line rows as (vertex, wall), along rows as (vertex, anchor). */
static std::pair<std::vector<std::pair<uint32_t, uint32_t>>, std::vector<std::pair<uint32_t, uint32_t>>> FailingPocketRows(const std::array<std::array<int, 2>, 7> &V, bool pocket)
{
	std::vector<PlanarAlongRow> along;
	std::pair<std::vector<std::pair<uint32_t, uint32_t>>, std::vector<std::pair<uint32_t, uint32_t>>> failing;
	for (const PlanarClearRow &r : PlanarCellClearRows(POCKET_CELL, POCKET_INSERT, pocket, &along)) {
		const std::array<int, 2> from = V[(r.wall + 6) % 7];
		const std::array<int, 2> normal = PlanarModel::STEP[(POCKET_CELL[r.wall].heading + 6) % 8];
		if ((V[r.vertex][0] - from[0]) * normal[0] + (V[r.vertex][1] - from[1]) * normal[1] < r.spacing) failing.first.emplace_back(r.vertex, r.wall);
	}
	for (const PlanarAlongRow &r : along) {
		const std::array<int, 2> step = PlanarModel::STEP[r.heading];
		if ((V[r.vertex][0] - V[r.anchor][0]) * step[0] + (V[r.vertex][1] - V[r.anchor][1]) * step[1] < r.spacing) failing.second.emplace_back(r.vertex, r.anchor);
	}
	return failing;
}

TEST_CASE("planar draw: the pocket rule lets an adjacent side use a step's room", "[junction][planar][planardraw]")
{
	using Row = std::tuple<uint32_t, uint32_t, int>;
	using Along = std::tuple<uint32_t, uint32_t, int, int>;
	const std::vector<Row> line = {{0, 3, 2}, {0, 4, 2}, {0, 5, 2}, {0, 6, 1}, {1, 3, 2}, {1, 4, 2}, {1, 5, 2}, {1, 6, 1}, {2, 5, 2}, {2, 6, 1}, {3, 0, 1}, {3, 2, 1}, {3, 5, 1}, {3, 6, 1}, {4, 0, 2}, {4, 2, 2}, {4, 6, 1}, {5, 0, 2}, {5, 2, 2}, {5, 3, 2}, {5, 4, 2}, {6, 3, 2}, {6, 4, 2}, {6, 5, 2}};
	std::vector<PlanarAlongRow> along;
	std::vector<Row> got;
	for (const PlanarClearRow &r : PlanarCellClearRows(POCKET_CELL, POCKET_INSERT, false, &along)) got.emplace_back(r.vertex, r.wall, r.spacing);
	std::ranges::sort(got);
	CHECK(got == line);
	CHECK(along.empty());

	got.clear();
	for (const PlanarClearRow &r : PlanarCellClearRows(POCKET_CELL, POCKET_INSERT, true, &along)) got.emplace_back(r.vertex, r.wall, r.spacing);
	std::ranges::sort(got);
	std::vector<Row> want = line;
	std::erase(want, Row{3, 0, 1});
	CHECK(got == want);
	std::vector<Along> got_along;
	for (const PlanarAlongRow &r : along) got_along.emplace_back(r.vertex, r.anchor, r.heading, r.spacing);
	std::ranges::sort(got_along);
	CHECK(got_along == std::vector<Along>{{3, 1, 0, 0}, {4, 1, 0, 0}, {5, 0, 4, 0}});

	const std::array<std::array<int, 2>, 7> good = {{{11, 16}, {15, 12}, {15, 6}, {13, 8}, {9, 12}, {9, 20}, {11, 20}}};
	const auto off = FailingPocketRows(good, false);
	CHECK(off.first == std::vector<std::pair<uint32_t, uint32_t>>{{3, 0}});
	CHECK(off.second.empty());
	const auto on = FailingPocketRows(good, true);
	CHECK(on.first.empty());
	CHECK(on.second.empty());
	const std::array<std::array<int, 2>, 7> back = {{{11, 16}, {15, 12}, {15, 6}, {13, 13}, {9, 12}, {9, 20}, {11, 20}}};
	const auto behind = FailingPocketRows(back, true);
	CHECK(std::ranges::find(behind.second, std::make_pair(3u, 1u)) != behind.second.end());
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

TEST_CASE("planar draw: PSG 197 base LP, its conflict", "[.][planarkeepapart]")
{
	PlanarShapeVariant shaped;
	PlanarDrawPins pins;
	if (!LoadPinned197(shaped, pins)) return;
	const std::vector<std::string> all = {"point", "axis", "diagonal", "param", "cross", "other"};
	PlanarDrawPins none = pins;
	none.pins.clear();
	PlanarDrawPins p8 = pins;
	p8.window = 8;
	for (const auto &[name, p] : std::vector<std::pair<std::string, const PlanarDrawPins *>>{{"no pins", &none}, {"pins W8", &p8}}) {
		const PlanarDrawResult r = CheckPlanarDrawable(shaped, 30.0, {.clearance = false, .pins = p, .continuous = all});
		WARN(fmt::format("base LP {}: status {}, {:.0f} ms", name, r.status, r.ms));
		for (const std::string &tag : r.conflict) WARN(fmt::format("base LP {} conflict: {}", name, tag));
		const PlanarDrawResult e = CheckPlanarDrawable(shaped, 30.0, {.clearance = false, .elastic = true, .pins = p});
		WARN(fmt::format("base elastic {}: status {}, slack {:.3f}", name, e.status, e.slack));
		for (const auto &[tag, amount] : e.stretched) WARN(fmt::format("base elastic {} stretched: {} {:.3f}", name, tag, amount));
		const PlanarTrackMap &map = shaped.structure.map;
		for (const auto &[tag, amount] : e.stretched) {
			if (!tag.starts_with("cross ")) continue;
			uint32_t c = UINT32_MAX;
			std::from_chars(tag.data() + 6, tag.data() + tag.size(), c);
			if (c < map.crossings.size()) WARN(fmt::format("base elastic {} crossing {}: {} x {}", name, c, map.tracks[map.crossings[c].a].name, map.tracks[map.crossings[c].b].name));
		}
	}
}
