/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_plan.cpp Test the planar model end to end: seed, placement, render, levels, signals and the checker (DESIGN-planar §5). */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/junction_route.h"
#include "../../../junction/junction_rules.h"
#include "../../../junction/junction_spec.h"
#include "../../../junction/planar/planar_plan.h"
#include "../../../junction/planar/planar_topology.h"
#include "../../../junction/signals/junction_signals.h"
#include "../../../junction/junction_layout.h"
#include "../../../junction/terrain/junction_terrain.h"

#include "../../../slope_func.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <chrono>
#include <set>
#include <tuple>
#include <string>

#include "../../../safeguards.h"

namespace {

/** The axis trackdir of a probe heading (0 N, 2 E, 4 S, 6 W). */
Trackdir AxisTrackdirForProbe(int heading)
{
	switch (heading) {
		case 2: return Trackdir::X_SW;
		case 4: return Trackdir::Y_SE;
		case 6: return Trackdir::X_NE;
		default: return Trackdir::Y_NW;
	}
}

/** A flat snapshot around a plan's box. */
JunctionTerrain FlatTerrain(int width, int height)
{
	JunctionTerrain t;
	t.origin = {0, 0};
	t.width = width;
	t.height = height;
	t.tiles.assign(static_cast<size_t>(width) * height, JunctionTerrainTile{});
	t.foundation_cost = 250;
	t.build_on_slopes = true;
	return t;
}

/** The ridge fixture: flat at 0, a ridge across the branch area rising over row r (SE), level at 1 over rows r + 1 and r + 2, falling over row r + 3 (NW). */
JunctionTerrain RidgeTerrain(int width, int height, int r)
{
	JunctionTerrain t = FlatTerrain(width, height);
	const auto row = [&](int y, int z_min, int z_max, DiagDirection incline) {
		for (int x = 0; x < width; x++) {
			JunctionTerrainTile &tile = t.tiles[static_cast<size_t>(y) * width + x];
			tile.z_min = static_cast<int8_t>(z_min);
			tile.z_max = static_cast<int8_t>(z_max);
			tile.incline = incline;
			tile.slope = incline == DiagDirection::Invalid ? SLOPE_FLAT : InclinedSlope(incline);
		}
	};
	row(r, 0, 1, DiagDirection::SE);
	row(r + 1, 1, 1, DiagDirection::Invalid);
	row(r + 2, 1, 1, DiagDirection::Invalid);
	row(r + 3, 0, 1, DiagDirection::NW);
	return t;
}

/** The seed, required solved. */
std::shared_ptr<const PlanarSeed> Seed(const std::string &text, const JunctionSpec &spec)
{
	std::shared_ptr<const PlanarSeed> seed = PlanarSeedFor(text, spec);
	INFO("seed: " << seed->why);
	REQUIRE(seed->why.empty());
	return seed;
}

/** Place, and require the layout and a clean checker. */
void RequirePlacedClean(const PlanarSeed &seed, const JunctionSpec &spec, const JunctionTerrain *terrain, TileIndexDiffC offset)
{
	const PlanarPlan plan = PlanPlanarJunction(seed, spec, terrain, offset, false);
	INFO("plan: " << plan.why);
	REQUIRE(plan.layout.has_value());
	INFO(RenderJunctionAscii(*plan.layout));
	const std::vector<RuleViolation> violations = CheckJunctionRules(*plan.layout, spec, PlanarCheckerTopology(seed.topology));
	for (const RuleViolation &v : violations) UNSCOPED_INFO(v.message << " at " << v.tile.x << "," << v.tile.y);
	CHECK(violations.empty());
}

}

TEST_CASE("planar: the T at one lane places on flat ground, checker clean, seed under 5 s", "[junction][planar][slow]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::shared_ptr<const PlanarSeed> seed = Seed(PlanarTText(1), spec);
	CHECK(seed->micros < 5'000'000);
	RequirePlacedClean(*seed, spec, nullptr, {0, 0});
}

TEST_CASE("planar: the T at two lanes places on flat ground, checker clean, seed under 5 s", "[junction][planar][slow]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::shared_ptr<const PlanarSeed> seed = Seed(PlanarTText(2), spec);
	CHECK(seed->micros < 5'000'000);
	RequirePlacedClean(*seed, spec, nullptr, {0, 0});
}

TEST_CASE("planar: the T at one lane places on the ridge, checker clean", "[junction][planar][slow]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::shared_ptr<const PlanarSeed> seed = Seed(PlanarTText(1), spec);
	const int margin = 4;
	const JunctionTerrain ridge = RidgeTerrain(seed->geometry.width + 2 * margin, seed->geometry.height + 2 * margin, margin + seed->geometry.height / 2);
	RequirePlacedClean(*seed, spec, &ridge, {static_cast<int16_t>(margin), static_cast<int16_t>(margin)});
}

TEST_CASE("planar: the two-lane LL² any hub places, checker clean", "[junction][planar][slow]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::shared_ptr<const PlanarSeed> seed = Seed(PlanarLL2Text(), spec);
	RequirePlacedClean(*seed, spec, nullptr, {0, 0});
}

TEST_CASE("planar: hover on the T at one lane", "[junction][planar][.bench]")
{
	const JunctionSpec spec = MakeTestJunctionSpec();
	const std::shared_ptr<const PlanarSeed> seed = Seed(PlanarTText(1), spec);
	const JunctionTerrain flat = FlatTerrain(seed->geometry.width + 16, seed->geometry.height + 16);
	constexpr int RUNS = 20;
	const auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < RUNS; i++) {
		const JunctionHoverPlan plan = PlanPlanarHoverNear(*seed, spec, flat, {static_cast<int16_t>(2 + i % 4), 4});
		INFO(plan.why);
		REQUIRE(plan.layout.has_value());
	}
	const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / RUNS;
	WARN("planar T1 hover: " << ms << " ms per plan");
	CHECK(ms < 100);
}

TEST_CASE("planar: LL² any has the composed hub's lane paths, prio on the fixed lane", "[junction][planar]")
{
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(PlanarLL2Text(), &error);
	INFO(error);
	REQUIRE(error.empty());
	CHECK(topology.routes.size() == 12);
	CHECK(std::ranges::count_if(topology.routes, [](const PlanarRoute &r) { return r.prio; }) == 6);
	const JunctionTopology checker = PlanarCheckerTopology(topology);
	REQUIRE(checker.groups.size() == 6);
	CHECK(checker.movements.size() == 6);
	/* The hub's lane paths (ComposeJunctionHub on the width-2 T, read from its lane path list): (from slot, to slot, prio). */
	const std::set<std::tuple<int, int, bool>> hub = {
		{0, 11, true}, {0, 6, false}, {0, 7, false}, {1, 11, false}, {1, 10, false}, {1, 6, true},
		{4, 11, false}, {4, 10, false}, {4, 3, true}, {5, 10, true}, {5, 2, false}, {5, 3, false},
		{8, 2, false}, {8, 3, false}, {8, 7, true}, {9, 2, true}, {9, 6, false}, {9, 7, false},
	};
	std::set<std::tuple<int, int, bool>> got;
	for (const JunctionLanePath &p : checker.lane_paths) got.insert({p.from, p.to, p.prio});
	CHECK(checker.lane_paths.size() == 18);
	CHECK(got == hub);
}

/* Probe hook (hidden): route programs exported by the Python probes (JUNCTION_PROBE_ROUTES; docs/junction-tool/wip/planar-probe8.py --routes) through the planar back end's signal pass and the checker. */
TEST_CASE("planar: probe routes, signals and checker", "[.probe]")
{
	const char *file = std::getenv("JUNCTION_PROBE_ROUTES");
	REQUIRE(file != nullptr);
	const char *topo = std::getenv("JUNCTION_PROBE_TOPOLOGY");
	const std::string text = topo != nullptr && std::string(topo) == "LL2" ? PlanarLL2Text() : PlanarTText(2);
	std::string error;
	const PlanarTopology topology = ParsePlanarTopology(text, &error);
	REQUIRE(error.empty());
	const JunctionSpec spec = MakeTestJunctionSpec();
	std::ifstream in(file);
	std::string word;
	JunctionLayout layout;
	int w, h;
	in >> word >> w >> h;
	layout.size = {static_cast<int16_t>(w), static_cast<int16_t>(h)};
	std::vector<JunctionTrackRoute> routes;
	JunctionSignalNetwork network;
	std::string line;
	std::getline(in, line);
	while (std::getline(in, line)) {
		std::istringstream head(line);
		std::string name;
		int copy, count;
		std::string line_names;
		head >> word >> name >> copy >> count >> line_names;
		std::vector<std::pair<bool, size_t>> copy_switches;
		std::string kind;
		size_t at;
		while (head >> kind >> at) copy_switches.emplace_back(kind == "S", at);
		struct P { int x, y, h; char k; int li; };
		std::vector<P> ps(count);
		for (P &p : ps) {
			std::getline(in, line);
			std::istringstream s(line);
			s >> p.x >> p.y >> p.h >> p.k >> p.li;
		}
		uint32_t r = 0;
		while (r < topology.routes.size() && topology.routes[r].name != name) r++;
		REQUIRE(r < topology.routes.size());
		uint16_t index = 0;
		for (uint32_t r2 = 0; r2 < r; r2++) index += static_cast<uint16_t>(topology.routes[r2].paths.size());
		size_t pi = 0;
		for (; pi < topology.routes[r].paths.size(); pi++) {
			std::string joined;
			for (uint32_t l : topology.routes[r].paths[pi]) joined += (joined.empty() ? "" : ",") + topology.lines[l].name;
			if (joined == line_names) break;
		}
		INFO(name << " lines " << line_names);
		REQUIRE(pi < topology.routes[r].paths.size());
		index += static_cast<uint16_t>(pi);
		const std::vector<uint32_t> &path = topology.routes[r].paths[pi];
		JunctionRoute program{{static_cast<int16_t>(ps[0].x), static_cast<int16_t>(ps[0].y)}, AxisTrackdirForProbe(ps[0].h), {}};
		for (size_t i = 1; i < ps.size(); i++) {
			if (ps[i].k == 'H') {
				size_t far = i + 1;
				while (far < ps.size() && ps[far].k == 'M') far++;
				REQUIRE(far < ps.size());
				{ std::ostringstream b; b << "B" << (far - i - 1); program.moves += b.str(); }
				i = far;
				continue;
			}
			const int turn = (ps[i].h - ps[i - 1].h + 8) % 8;
			program.moves += turn == 0 ? 'F' : turn == 1 ? 'L' : 'R'; // the rasteriser's L is +y of +x travel
		}
		const JunctionRaster raster = RasterizeJunctionRoute(program);
		INFO(name << " copy " << copy << ": " << raster.error << " moves " << program.moves);
		REQUIRE(raster.error.empty());
		REQUIRE(raster.pieces.size() == ps.size());
		for (size_t i = 0; i < ps.size(); i++) {
			INFO("piece " << i << " renders " << raster.pieces[i].tile.x << "," << raster.pieces[i].tile.y << " planned " << ps[i].x << "," << ps[i].y);
			REQUIRE((raster.pieces[i].tile.x == ps[i].x && raster.pieces[i].tile.y == ps[i].y));
		}
		JunctionTrackRoute track{index, static_cast<uint8_t>(copy), raster.pieces, {}};
		for (const P &p : ps) track.tags.push_back({path[p.li], JunctionTrackPieceTag::RUN, 0, static_cast<uint8_t>(copy)});
		AddRasterToLayout(layout, raster);
		JunctionSignalRoute signal;
		signal.main = topology.routes[r].prio;
		signal.group = static_cast<uint32_t>(name.front());
		for (size_t k = 0; k + 1 < path.size(); k++) {
			const uint32_t node = topology.lines[path[k]].to.index;
			size_t first = 0;
			while (ps[first].li <= static_cast<int>(k)) first++;
			signal.arrivals.emplace_back(node, first - 1);
			if (topology.nodes[node].IsSplit()) {
				signal.switches.emplace_back(true, first);
			} else {
				signal.switches.emplace_back(false, first - 1);
				signal.merge_nodes.emplace_back(first - 1, node);
			}
		}
		for (const auto &s : copy_switches) signal.switches.push_back(s);
		std::ranges::sort(signal.switches, [](const auto &a, const auto &b) { return a.second != b.second ? a.second < b.second : a.first > b.first; });
		network.routes.push_back(std::move(signal));
		routes.push_back(std::move(track));
	}

	int16_t mx = 0, my = 0;
	for (const JunctionTrack &t : layout.tracks) { mx = std::max(mx, t.offset.x); my = std::max(my, t.offset.y); }
	layout.size = {static_cast<int16_t>(mx + 1), static_cast<int16_t>(my + 1)};
	std::string why;
	SignalJunctionTrackRoutes(layout, routes, network, spec, &why);
	INFO("signals: " << why);
	if (const char *out = std::getenv("JUNCTION_PROBE_OUT"); out != nullptr) {
		std::ofstream(out) << JunctionLayoutToJson(layout);
	}
	const std::vector<RuleViolation> violations = CheckJunctionRules(layout, spec, PlanarCheckerTopology(topology));
	if (const char *out = std::getenv("JUNCTION_PROBE_VIOLATIONS"); out != nullptr) {
		std::ofstream o(out);
		for (const RuleViolation &v : violations) o << static_cast<int>(v.rule) << " " << v.tile.x << "," << v.tile.y << " " << v.message << "\n";
	}
	CHECK(why.empty());
	CHECK(violations.size() == 0);
}
