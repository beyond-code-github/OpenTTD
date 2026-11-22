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
#include "../../../junction/planar/planar_plan.h"
#include "../../../junction/planar/planar_shape_layer.h"
#include "../../../junction/planar/planar_structure.h"
#include "../../../junction/planar/planar_topology.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <filesystem>
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

/**
 * Every bay of a drawing (SPEC-slots-lengths §8.1): per non-prio route path, the travel from its last topology split or port to each topology merge.
 * @param c The case.
 * @param r Its drawing.
 * @return The bays, in half-tile units.
 */
static std::vector<double> DrawnBays(const DrawCase &c, const PlanarDrawResult &r)
{
	const PlanarTrackMap &map = c.v.structure.map;
	const auto site_of = [&](size_t n) -> const PlanarStructureSite * {
		const PlanarStructureSite *site = nullptr;
		for (const PlanarStructureSite &candidate : c.v.structure.sites) {
			if (candidate.node == n) site = &candidate;
		}
		return site;
	};
	const auto is_split = [&](size_t n) {
		const PlanarNode &node = map.expanded.nodes[n];
		const PlanarStructureSite *site = site_of(n);
		return node.in.size() == 1 && (node.out.size() != 1 || (site != nullptr && site->split));
	};
	const auto topology_node = [&](size_t n) {
		const PlanarNode &node = map.expanded.nodes[n];
		for (const uint32_t t : node.in) if (map.part_of[t] == UINT32_MAX) return false;
		for (const uint32_t t : node.out) if (map.part_of[t] == UINT32_MAX) return false;
		return true;
	};
	std::vector<double> bays;
	for (const PlanarRoute &route : map.expanded.routes) {
		if (route.prio) continue;
		for (const std::vector<uint32_t> &path : route.paths) {
			double acc = 0;
			double from_split = 0;
			for (const uint32_t t : path) {
				for (const double piece : r.lengths[t]) acc += piece;
				const PlanarEnd &to = map.expanded.lines[t].to;
				if (to.port || !topology_node(to.index)) continue;
				if (is_split(to.index)) {
					from_split = acc;
				} else {
					bays.push_back(acc - from_split);
				}
			}
		}
	}
	return bays;
}

TEST_CASE("planar draw: bays hold the route from a split or port to a merge", "[junction][planar][planardraw]")
{
	const DrawCase c = ShapeFirst(PlanarTText(1), 1);
	const PlanarDrawResult plain = CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .bays = false});
	const PlanarDrawResult bayed = CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .bays = true});
	REQUIRE(plain.feasible);
	REQUIRE(bayed.feasible);
	const std::vector<double> without = DrawnBays(c, plain);
	const std::vector<double> with = DrawnBays(c, bayed);
	const double bay = c.v.structure.rules.bay;
	REQUIRE(!without.empty());
	REQUIRE(without.size() == with.size());
	INFO("rules.bay " << bay);
	CHECK(std::ranges::any_of(without, [&](double b) { return b < bay; }));
	for (const double b : with) CHECK(b >= bay);
}

TEST_CASE("planar draw: elastic mode names a short bay", "[junction][planar][planardraw]")
{
	const DrawCase c = ShapeFirst(PlanarTText(1), 1);
	const PlanarDrawResult plain = CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .bays = false});
	REQUIRE(plain.feasible);
	const PlanarDrawResult result = CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .bays = true, .elastic = true, .elastic_families = {"bay"}, .fix_points = &plain.points, .fix_window = 0});
	CHECK(std::ranges::any_of(result.stretched, [](const auto &s) { return s.first.starts_with("bay "); }));
}

/**
 * Every gap between a route path's bends and each of the next three (SPEC-slots-lengths §8.2, Curve rows).
 * @param c The case.
 * @param r Its drawing.
 * @param corridor Whether a beside track's opening and closing steps are bend-generating virtual pieces of length 2.
 * @param committed When given, a flex middle whose length in this reference is at least 0.25 is a real piece for the bends.
 * @return Per gap, its travel in half tiles and the least the rules ask.
 */
static std::vector<std::pair<double, int>> DrawnCurveGaps(const DrawCase &c, const PlanarDrawResult &r, bool corridor = false, const std::vector<std::vector<double>> *committed = nullptr)
{
	const PlanarTrackMap &map = c.v.structure.map;
	const PlanarRuleFields &rules = c.v.structure.rules;
	std::vector<std::pair<double, int>> gaps;
	for (const PlanarRoute &route : map.expanded.routes) {
		for (const std::vector<uint32_t> &path : route.paths) {
			std::vector<std::pair<double, bool>> bends;
			double acc = 0;
			int last = -1;
			const auto virtual_piece = [&](int heading) {
				if (last != -1 && last != heading) bends.emplace_back(acc, PlanarModel::Rot(last, heading) > 0);
				last = heading;
				acc += 2;
			};
			for (const uint32_t t : path) {
				const std::vector<uint32_t> &flex = c.v.tracks[t].flex;
				const std::vector<uint32_t> &steps = c.v.tracks[t].steps;
				const std::vector<int> &pieces = c.v.tracks[t].pieces;
				int side = 0;
				if (corridor && map.part_of[t] == UINT32_MAX) {
					std::vector<uint32_t> chain;
					REQUIRE(ParentChain(map, c.v.structure.sites, t, chain));
					side = map.Side(t, 0, chain[0], 0);
					virtual_piece((((pieces.front() + side) % 8) + 8) % 8);
				}
				for (size_t i = 0; i < pieces.size(); i++) {
					const bool real_flex = committed != nullptr && std::ranges::find(flex, i) != flex.end() && t < committed->size() && i < (*committed)[t].size() && (*committed)[t][i] >= 0.25;
					const bool middle = (std::ranges::find(flex, i) != flex.end() && !real_flex) || std::ranges::find(steps, i) != steps.end();
					const int heading = ((pieces[i] % 8) + 8) % 8;
					if (!middle) {
						if (last != -1 && last != heading) bends.emplace_back(acc, PlanarModel::Rot(last, heading) > 0);
						last = heading;
					}
					acc += r.lengths[t][i];
				}
				if (side != 0) virtual_piece((((pieces.back() - side) % 8) + 8) % 8);
			}
			for (size_t i = 0; i < bends.size(); i++) {
				for (size_t k = 1; k <= 3 && i + k < bends.size(); k++) {
					const int least = k == 1 ? (bends[i].second != bends[i + k].second ? rules.curve_opposite : rules.curve_same) : rules.curve_window[k - 2];
					gaps.emplace_back(bends[i + k].first - bends[i].first, least);
				}
			}
		}
	}
	return gaps;
}

TEST_CASE("planar draw: curves keep their spacing along a route", "[junction][planar][planardraw]")
{
	const DrawCase c = ShapeFirst(PlanarTText(1), 1);
	const PlanarDrawResult plain = CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .curves = false});
	const PlanarDrawResult curved = CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .curves = true});
	REQUIRE(plain.feasible);
	REQUIRE(curved.feasible);
	const auto without = DrawnCurveGaps(c, plain);
	const auto with = DrawnCurveGaps(c, curved);
	REQUIRE(!without.empty());
	CHECK(std::ranges::any_of(without, [](const auto &g) { return g.first < g.second; }));
	for (const auto &[gap, least] : with) {
		INFO("gap " << gap << " least " << least);
		CHECK(gap >= least);
	}
}

TEST_CASE("planar draw: a prio row's corridor steps keep curve spacing", "[junction][planar][planardraw]")
{
	const DrawCase c = ShapeFirst(PlanarTText(1), 1);
	const PlanarDrawResult r = CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .curves = true, .corridor = true});
	REQUIRE(r.feasible);
	const auto gaps = DrawnCurveGaps(c, r, true);
	REQUIRE(!gaps.empty());
	for (const auto &[gap, least] : gaps) {
		INFO("gap " << gap << " least " << least);
		CHECK(gap >= least);
	}
}

TEST_CASE("planar draw: a committed flex insert keeps curve spacing", "[junction][planar][planardraw]")
{
	const DrawCase c = ShapeFirst(PlanarTText(1), 1);
	const PlanarDrawResult first = CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .curves = true});
	REQUIRE(first.feasible);
	/* The T needs several inserts for its base geometry: every flex middle is committed at 2 half tiles. */
	std::vector<std::vector<double>> reference = first.lengths;
	size_t middles = 0;
	for (size_t t = 0; t < c.v.tracks.size(); t++) {
		for (const uint32_t m : c.v.tracks[t].flex) {
			reference[t][m] = 2;
			middles++;
		}
	}
	REQUIRE(middles > 0);
	const PlanarDrawResult r = CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .curves = true, .curve_flex_from = &reference});
	std::string conflicts;
	for (const std::string &tag : r.conflict) conflicts += tag + "; ";
	INFO("status " << r.status << " conflict " << conflicts);
	REQUIRE(r.feasible);
	for (size_t t = 0; t < c.v.tracks.size(); t++) {
		for (const uint32_t m : c.v.tracks[t].flex) {
			INFO("flex middle " << t << " " << m);
			CHECK(r.lengths[t][m] >= 2);
		}
	}
	for (const auto &[gap, least] : DrawnCurveGaps(c, r, false, &reference)) {
		INFO("gap " << gap << " least " << least);
		CHECK(gap >= least);
	}

	/* A recorded measurement: the free drawing's own lengths as the commitment. */
	size_t committed = 0;
	for (size_t t = 0; t < c.v.tracks.size(); t++) {
		for (const uint32_t m : c.v.tracks[t].flex) committed += first.lengths[t][m] >= 0.25;
	}
	const PlanarDrawResult own = CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .curves = true, .curve_flex_from = &first.lengths});
	WARN("own-lengths commitment: status " << own.status << ", feasible " << own.feasible << ", committed " << committed << " of " << middles);
}

TEST_CASE("planar draw: the pipeline draws the one-lane T with optional flex (D422)", "[junction][planar][planardraw]")
{
	const DrawCase c = ShapeFirst(PlanarTText(1), 1);
	const PlanarDrawPipelineResult r = DrawPlanarPipeline(c.v, {});
	INFO("stopped " << r.stopped);
	REQUIRE(r.final.status == "Optimal");
	for (const auto &[gap, least] : DrawnCurveGaps(c, r.final, false, &r.committed)) {
		INFO("gap " << gap << " least " << least);
		CHECK(gap >= least - 1e-6); // HiGHS's feasibility tolerance: the final's lengths are whole up to it.
	}
	for (size_t t = 0; t < c.v.tracks.size(); t++) {
		for (const uint32_t m : c.v.tracks[t].flex) {
			if (t >= r.committed.size() || m >= r.committed[t].size() || r.committed[t][m] >= 1.0) continue;
			INFO("flex middle " << t << " " << m);
			CHECK(r.final.lengths[t][m] == 0);
		}
	}
}

TEST_CASE("planar draw: facing pairs take their side from the face walk on T2", "[planardraw]")
{
	const DrawCase c = ShapeFirst(PlanarTText(2), 1);
	PlanarDrawPipelineOptions o;
	o.facing_sides = true;
	o.final_seconds = 5;
	const PlanarDrawPipelineResult r = DrawPlanarPipeline(c.v, o);
	CHECK(r.last_round.facing_fixed >= 1);
	for (const auto &[tag, text] : r.last_round.sep_sides) {
		if (!text.ends_with(" facing")) continue;
		INFO(tag << ": " << text);
		CHECK(text.find("spacing") != std::string::npos);
		CHECK(text.find("from i") != std::string::npos);
	}
}

TEST_CASE("planar draw: the pipeline on T2, T3 and LL2", "[.][planarhubs]")
{
	const std::vector<std::pair<std::string, std::string>> hubs = {{"T1", PlanarTText(1)}, {"T2", PlanarTText(2)}, {"T3", PlanarTText(3)}, {"LL2", PlanarLL2Text()}};
	for (const auto &[name, text] : hubs) {
		const DrawCase c = ShapeFirst(text, 1);
		PlanarDrawPipelineOptions o;
		o.final_seconds = 15;
		/* Measurement aids: PLANAR_PROBE_SLACK_PRICE (commit on), PLANAR_PROBE_DISCOVERY_SLACK_PRICE (seed and discovery) PLANAR_PROBE_NO_CURVE_DISCOVERY (Scheme A), PLANAR_PROBE_SIDES (per-pair side probe) PLANAR_PROBE_SIBLINGS (sibling lanes as offset curves) and PLANAR_PROBE_FACING (D426 facing pairs). */
		if (const char *s = std::getenv("PLANAR_PROBE_SLACK_PRICE"); s != nullptr) o.slack_price = std::atof(s);
		if (const char *s = std::getenv("PLANAR_PROBE_DISCOVERY_SLACK_PRICE"); s != nullptr) o.discovery_slack_price = std::atof(s);
		o.free_stretched_sides = std::getenv("PLANAR_PROBE_FREE_SIDES") != nullptr;
		o.side_probe = std::getenv("PLANAR_PROBE_SIDES") != nullptr;
		o.sibling_lanes = std::getenv("PLANAR_PROBE_SIBLINGS") != nullptr;
		o.facing_sides = std::getenv("PLANAR_PROBE_FACING") != nullptr;
		o.inward_sides = std::getenv("PLANAR_PROBE_INWARD") != nullptr;
		o.curve_discovery = std::getenv("PLANAR_PROBE_NO_CURVE_DISCOVERY") == nullptr;
		if (const char *s = std::getenv("PLANAR_DUMP"); s != nullptr) {
			o.dump_dir = fmt::format("{}/{}", s, name);
			std::filesystem::create_directories(o.dump_dir);
		}
		const auto start = std::chrono::steady_clock::now();
		const PlanarDrawPipelineResult r = DrawPlanarPipeline(c.v, o);
		const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
		for (const std::string &line : r.log) WARN(fmt::format("{} {}", name, line));
		size_t pruned = 0, added = 0;
		pruned = r.pruned;
		added = r.added;
		WARN(fmt::format("{} committed {} pruned {} added {} stopped '{}'", name, r.committed_count, pruned, added, r.stopped));
		WARN(fmt::format("{} final {} ms {}", name, r.final.status, r.final.ms));
		for (const std::string &tag : r.final.conflict) WARN(name + " conflict " + tag);
		std::map<std::string, std::pair<size_t, double>> families;
		for (const auto &[tag, slack] : r.last_round.stretched) {
			auto &f = families[tag.substr(0, tag.find(' '))];
			f.first++;
			f.second += slack;
		}
		WARN(fmt::format("{} last round slack {:.3f}, stretched {}", name, r.last_round.slack, r.last_round.stretched.size()));
		if (o.facing_sides) WARN(fmt::format("{} facing: fixed {} choice {}", name, r.last_round.facing_fixed, r.last_round.facing_choice));
		if (o.inward_sides) WARN(fmt::format("{} inward: short {}", name, r.last_round.inward_short));
		if (o.sibling_lanes) {
			WARN(fmt::format("{} sibling lanes: pairs {} sep skipped {}", name, r.last_round.lane_pairs, r.last_round.lane_sep_skipped));
			for (const std::string &line : r.last_round.lane_skipped) WARN(fmt::format("{} sibling lanes skipped {}", name, line));
		}
		for (const auto &[family, f] : families) WARN(fmt::format("{} family {}: count {}, sum {:.3f}", name, family, f.first, f.second));
			{
				const PlanarTrackMap &map = c.v.structure.map;
				const std::vector<PlanarFaceWalls> face_walls = PlanarShapeFaceWalls(c.v);
				const auto piece_text = [&](uint32_t t, uint32_t piece) {
					if (t >= r.last_round.points.size() || piece + 1 >= r.last_round.points[t].size()) return std::string("?");
					const auto &a = r.last_round.points[t][piece];
					const auto &b = r.last_round.points[t][piece + 1];
					return fmt::format("{}#{} h{} ({},{})-({},{})", map.tracks[t].name, piece, ((c.v.tracks[t].pieces[piece] % 8) + 8) % 8, a[0], a[1], b[0], b[1]);
				};
				size_t sep_rows = 0;
				for (const auto &[tag, amount] : r.last_round.stretched) {
					std::istringstream words(tag);
					std::string family;
					uint32_t face = 0, i = 0, j = 0;
					words >> family;
					if (family != "sep" || !(words >> face >> i >> j)) continue;
					if (++sep_rows > 30) continue;
					for (const PlanarFaceWalls &f : face_walls) {
						if (f.face != face || i >= f.walls.size() || j >= f.walls.size()) continue;
						const PlanarCellWall &a = f.walls[i];
						const PlanarCellWall &b = f.walls[j];
						WARN(fmt::format("{} decode {} {:.2f}: {} | {}", name, tag, amount, a.track < c.v.tracks.size() ? piece_text(a.track, a.piece) : std::string("box"), b.track < c.v.tracks.size() ? piece_text(b.track, b.piece) : std::string("box")));
					}
				}
				if (sep_rows > 30) WARN(fmt::format("{} decode: {} sep rows, first 30 shown", name, sep_rows));
				if (name != "T2") {
					for (const auto &[tag, amount] : r.last_round.stretched) {
						if (!tag.starts_with("sep ")) continue;
						for (size_t n = 0; n < r.rounds.size(); n++) {
							std::string side = "none";
							for (const auto &[t, text] : r.rounds[n].sep_sides) {
								if (t == tag) side = text;
							}
							double amt = 0;
							for (const auto &[t, x] : r.rounds[n].stretched) {
								if (t == tag) amt = x;
							}
							const std::string &line = n < r.log.size() ? r.log[n] : std::string("?");
							WARN(fmt::format("{} {} round {} ({}): side {}, stretched {:.3f}", name, tag, n, line.substr(0, line.find(':')), side, amt));
						}
					}
				}
				const auto list_of = [](const auto &xs) {
					std::string s;
					for (const auto x : xs) s += fmt::format(" {}", x);
					return s;
				};
				if (o.side_probe) {
					/* Step (a): per track of a stretched sep pair, its pieces, flex, steps and whether it is a beside track or has some. */
					std::map<uint32_t, std::vector<std::string>> beside_of;
					std::set<uint32_t> is_beside;
					for (uint32_t b = 0; b < map.tracks.size(); b++) {
						if (map.part_of[b] != UINT32_MAX) continue;
						is_beside.insert(b);
						std::vector<uint32_t> chain;
						if (!ParentChain(map, c.v.structure.sites, b, chain)) continue;
						for (const uint32_t t : chain) beside_of[t].push_back(map.tracks[b].name);
					}
					std::set<uint32_t> printed;
					for (const auto &[tag, amount] : r.last_round.stretched) {
						std::istringstream words(tag);
						std::string family;
						uint32_t face = 0, i = 0, j = 0;
						words >> family;
						if (family != "sep" || !(words >> face >> i >> j)) continue;
						for (const PlanarFaceWalls &f : face_walls) {
							if (f.face != face || i >= f.walls.size() || j >= f.walls.size()) continue;
							for (const PlanarCellWall *wall : {&f.walls[i], &f.walls[j]}) {
								const uint32_t t = wall->track;
								if (t >= c.v.tracks.size() || !printed.insert(t).second) continue;
								std::string list;
								for (const std::string &n : beside_of[t]) list += (list.empty() ? "" : ",") + n;
								WARN(fmt::format("{} track {}: pieces{}; flex{}; steps{}; stretched piece {}; beside of {}; is beside {}", name, map.tracks[t].name, list_of(c.v.tracks[t].pieces), list_of(c.v.tracks[t].flex), list_of(c.v.tracks[t].steps), wall->piece, list.empty() ? "none" : list, is_beside.contains(t) ? "yes" : "no"));
							}
						}
					}
				}
				size_t beside = 0;
				for (uint32_t b = 0; b < map.tracks.size(); b++) {
					if (map.part_of[b] != UINT32_MAX) continue;
					if (++beside > 30) continue;
					std::vector<uint32_t> chain;
					const bool ok = ParentChain(map, c.v.structure.sites, b, chain);
					const auto &ln = map.expanded.lines[b];
					std::string text = fmt::format("{} beside {}: from {} {} to {} {}; pieces{}; flex{}; steps{}; chain{}", name, map.tracks[b].name, ln.from.port ? "port" : "node", ln.from.index, ln.to.port ? "port" : "node", ln.to.index, list_of(c.v.tracks[b].pieces), list_of(c.v.tracks[b].flex), list_of(c.v.tracks[b].steps), ok ? "" : " NONE");
					for (const uint32_t t : chain) text += fmt::format(" {}[pieces{}; flex{}; steps{}]", map.tracks[t].name, list_of(c.v.tracks[t].pieces), list_of(c.v.tracks[t].flex), list_of(c.v.tracks[t].steps));
					WARN(text);
				}
				if (beside > 30) WARN(fmt::format("{} beside tracks: {}, first 30 shown", name, beside));
			}
		if (o.free_stretched_sides && r.rounds.size() >= 2) {
			/* log_round pushes the free-sides round just before the final. */
			const PlanarDrawResult &fr = r.rounds[r.rounds.size() - 2];
			size_t freed = 0;
			for (const auto &[tag, amount] : r.last_round.stretched) freed += tag.starts_with("sep ") ? 1 : 0;
			WARN(fmt::format("{} free sides: status {}, slack {:.3f}, sep tags freed {}, stretched rows {}", name, fr.status, fr.slack, freed, fr.stretched.size()));
			for (const auto &[tag, amount] : fr.stretched) WARN(fmt::format("{} free sides stretched {} {:.3f}", name, tag, amount));
		}
		WARN(fmt::format("{} wall {:.1f} s", name, wall));
	}
}

/** The decoded piece of a wall: track#piece h<heading> with its end points when a drawing is given. */
static std::string CertWallText(const DrawCase &c, const PlanarCellWall &w, const std::vector<std::vector<std::array<int, 2>>> *points)
{
	if (w.track >= c.v.tracks.size()) return w.track == UINT32_MAX ? fmt::format("box {}", w.piece) : fmt::format("dummy {}", w.piece);
	const int heading = ((c.v.tracks[w.track].pieces[w.piece] % 8) + 8) % 8;
	std::string text = fmt::format("{}#{} h{}", c.v.structure.map.tracks[w.track].name, w.piece, heading);
	if (points != nullptr && w.track < points->size() && w.piece + 1 < (*points)[w.track].size()) {
		const auto &a = (*points)[w.track][w.piece];
		const auto &b = (*points)[w.track][w.piece + 1];
		text += fmt::format(" ({},{})-({},{})", a[0], a[1], b[0], b[1]);
	}
	return text;
}

/** Whether a line of a topology text starts with the prio statement. */
static std::string StripPrio(const std::string &text)
{
	std::istringstream in(text);
	std::string line, out;
	while (std::getline(in, line)) {
		const size_t first = line.find_first_not_of(" \t");
		if (first != std::string::npos && line.compare(first, 4, "prio") == 0) continue;
		out += line + "\n";
	}
	return out;
}

TEST_CASE("planar draw: crowding certificates on T3 and LL2", "[.][planarcert]")
{
	struct Case {
		std::string hub;
		std::vector<uint32_t> faces;
		double bound;
	};
	const std::vector<Case> cases = {
		{"T3", {75}, 256}, {"T3", {29}, 256}, {"T3", {66}, 256}, {"T3", {75, 29, 66}, 256}, {"T3", {75}, 4096},
		{"LL2", {33}, 256}, {"LL2", {34}, 256}, {"LL2", {37}, 256}, {"LL2", {33, 34, 37}, 256},
	};
	const std::map<std::string, std::vector<std::array<uint32_t, 3>>> stretched = {
		{"T3", {{75, 2, 15}, {75, 5, 8}, {29, 1, 10}, {29, 4, 9}}},
		{"LL2", {{37, 6, 12}, {33, 0, 9}, {33, 1, 9}, {33, 2, 9}, {33, 3, 10}, {33, 3, 8}, {34, 1, 9}, {34, 3, 7}, {37, 6, 11}}},
	};
	const std::map<std::string, std::string> texts = {{"T3", PlanarTText(3)}, {"LL2", PlanarLL2Text()}};
	std::map<std::string, DrawCase> built;
	for (const auto &[hub, text] : texts) built.emplace(hub, ShapeFirst(text, 1));
	const auto faces_text = [](const std::vector<uint32_t> &faces) {
		std::string s;
		for (const uint32_t f : faces) s += (s.empty() ? "" : ",") + fmt::format("{}", f);
		return s;
	};
	/* The walls of each face named, so the reader can check the face numbers. */
	std::map<std::string, std::map<uint32_t, bool>> exists;
	for (const auto &[hub, c] : built) {
		const std::vector<PlanarFaceWalls> face_walls = PlanarShapeFaceWalls(c.v);
		std::set<uint32_t> wanted;
		for (const Case &k : cases) {
			if (k.hub == hub) wanted.insert(k.faces.begin(), k.faces.end());
		}
		for (const uint32_t face : wanted) {
			exists[hub][face] = false;
			for (const PlanarFaceWalls &f : face_walls) {
				if (f.face != face) continue;
				exists[hub][face] = true;
				std::string line;
				for (size_t i = 0; i < f.walls.size(); i++) line += fmt::format(" [{}] {}", i, CertWallText(c, f.walls[i], nullptr));
				WARN(fmt::format("cert {} face {} walls:{}", hub, face, line));
			}
			if (!exists[hub][face]) WARN(fmt::format("cert {} face {} does not exist in this variant", hub, face));
		}
	}
	for (const Case &k : cases) {
		const DrawCase &c = built.at(k.hub);
		bool all = true;
		for (const uint32_t f : k.faces) all = all && exists[k.hub][f];
		if (!all) {
			WARN(fmt::format("cert {} faces {} B {}: skipped, a face does not exist", k.hub, faces_text(k.faces), k.bound));
			continue;
		}
		PlanarDrawOptions o;
		o.clearance = true;
		o.clear_rows = true;
		o.pairwise = true;
		o.pairwise_all_faces = true;
		o.only_faces = k.faces;
		o.corridor = true;
		o.tidy = false;
		o.continuous = {"point", "axis", "diagonal", "param", "cross"};
		o.coord_max = k.bound;
		const PlanarDrawResult r = CheckPlanarDrawable(c.v, 15.0, o);
		WARN(fmt::format("cert {} faces {} B {}: {} ms {:.0f} binaries {}", k.hub, faces_text(k.faces), k.bound, r.status, r.ms, r.binaries));
		if (std::getenv("PLANAR_PROBE_INWARD") != nullptr && k.bound == 256 && (k.hub == "T3" || (k.hub == "LL2" && k.faces == std::vector<uint32_t>{33}))) {
			PlanarDrawOptions oi = o;
			oi.inward_sides = true;
			const PlanarDrawResult ri = CheckPlanarDrawable(c.v, 15.0, oi);
			WARN(fmt::format("cert inward {} faces {}: {} ms {:.0f} binaries {}", k.hub, faces_text(k.faces), ri.status, ri.ms, ri.binaries));
		}
		const std::vector<PlanarFaceWalls> face_walls = PlanarShapeFaceWalls(c.v);
		const auto decode = [&](const std::string &tag) {
			std::istringstream words(tag);
			std::string family;
			uint32_t face = 0, i = 0, j = 0;
			words >> family;
			if (family != "sep" || !(words >> face >> i >> j)) return tag;
			for (const PlanarFaceWalls &f : face_walls) {
				if (f.face != face || i >= f.walls.size() || j >= f.walls.size()) continue;
				return fmt::format("{} | {} | {}", tag, CertWallText(c, f.walls[i], nullptr), CertWallText(c, f.walls[j], nullptr));
			}
			return tag;
		};
		for (const std::string &tag : r.conflict) WARN(fmt::format("cert {} faces {} conflict {}", k.hub, faces_text(k.faces), decode(tag)));
		if (r.status != "Optimal") continue;
		for (const auto &[face, i, j] : stretched.at(k.hub)) {
			if (std::ranges::find(k.faces, face) == k.faces.end()) continue;
			for (const PlanarFaceWalls &f : face_walls) {
				if (f.face != face || i >= f.walls.size() || j >= f.walls.size()) continue;
				WARN(fmt::format("cert {} faces {} sep {} {} {}: {} | {}", k.hub, faces_text(k.faces), face, i, j, CertWallText(c, f.walls[i], &r.points), CertWallText(c, f.walls[j], &r.points)));
			}
		}
	}

	/* (A) prio removed. */
	for (const auto &[hub, text] : texts) {
		std::string error;
		const PlanarTopology topology = ParsePlanarTopology(StripPrio(text), &error);
		if (!error.empty()) {
			WARN(fmt::format("noprio {}: the parser refuses the stripped text: {}", hub, error));
			continue;
		}
		const JunctionSpec spec = MakeTestJunctionSpec();
		const PlanarStructureResult sr = BuildPlanarStructure(topology, spec, 1);
		if (sr.variants.empty()) {
			WARN(fmt::format("noprio {}: Structure refuses the stripped text: {}", hub, sr.why));
			continue;
		}
		PlanarShapeVariant without;
		std::string why;
		if (!ShapePlanarStructureVariant(sr.variants[0], without, why)) {
			WARN(fmt::format("noprio {}: Shape refuses the stripped text: {}", hub, why));
			continue;
		}
		PlanarDrawPipelineOptions po;
		po.final_seconds = 15;
		const PlanarDrawPipelineResult r = DrawPlanarPipeline(without, po);
		std::map<std::string, std::pair<size_t, double>> families;
		for (const auto &[tag, slack] : r.last_round.stretched) {
			auto &f = families[tag.substr(0, tag.find(' '))];
			f.first++;
			f.second += slack;
		}
		std::string family_text;
		for (const auto &[family, f] : families) family_text += fmt::format("{}{}: {}, {:.3f}", family_text.empty() ? "" : "; ", family, f.first, f.second);
		WARN(fmt::format("noprio {}: last round slack {:.3f}, by family {}, final {}, stopped '{}'", hub, r.last_round.slack, family_text, r.final.status, r.stopped));
		DrawCase d;
		d.topology = topology;
		d.v = without;
		const std::vector<PlanarFaceWalls> face_walls = PlanarShapeFaceWalls(without);
		for (const auto &[tag, amount] : r.last_round.stretched) {
			std::istringstream words(tag);
			std::string family;
			uint32_t face = 0, i = 0, j = 0;
			words >> family;
			if (family != "sep" || !(words >> face >> i >> j)) continue;
			for (const PlanarFaceWalls &f : face_walls) {
				if (f.face != face || i >= f.walls.size() || j >= f.walls.size()) continue;
				WARN(fmt::format("noprio {} decode {} {:.2f}: {} | {}", hub, tag, amount, CertWallText(d, f.walls[i], &r.last_round.points), CertWallText(d, f.walls[j], &r.last_round.points)));
			}
		}
		/* Heading control against the variant with prio kept. */
		const DrawCase &with = built.at(hub);
		const auto list_text = [](const std::vector<int> &pieces) {
			std::string s = "[";
			for (size_t n = 0; n < pieces.size(); n++) s += fmt::format("{}{}", n == 0 ? "" : " ", ((pieces[n] % 8) + 8) % 8);
			return s + "]";
		};
		/* Names repeat (a line split into parts keeps its name), so the k-th track of a name in one variant meets the k-th in the other. */
		std::map<std::pair<std::string, size_t>, size_t> without_index;
		{
			std::map<std::string, size_t> seen;
			for (size_t t = 0; t < without.structure.map.tracks.size(); t++) {
				const std::string &name = without.structure.map.tracks[t].name;
				without_index[{name, seen[name]++}] = t;
			}
		}
		std::map<std::string, size_t> with_seen;
		size_t identical = 0, differ = 0;
		std::vector<std::string> lines;
		for (size_t t = 0; t < with.v.structure.map.tracks.size(); t++) {
			const std::string &name = with.v.structure.map.tracks[t].name;
			if (name.starts_with("prio:") || name.find('"') != std::string::npos) continue;
			const size_t ordinal = with_seen[name]++;
			const auto it = without_index.find({name, ordinal});
			if (it == without_index.end()) continue;
			const std::string a = list_text(std::vector<int>(with.v.tracks[t].pieces.begin(), with.v.tracks[t].pieces.end()));
			const std::string b = list_text(std::vector<int>(without.tracks[it->second].pieces.begin(), without.tracks[it->second].pieces.end()));
			if (a == b) {
				identical++;
			} else {
				differ++;
				lines.push_back(fmt::format("{}: with {} without {}", ordinal == 0 ? name : fmt::format("{} (part {})", name, ordinal), a, b));
			}
		}
		WARN(fmt::format("noprio {} headings: {} identical, {} differ", hub, identical, differ));
		for (const std::string &line : lines) WARN(fmt::format("noprio {} {}", hub, line));
	}
}

TEST_CASE("planar draw: LL2 face 33 infeasible core", "[.][planarcert]")
{
	const DrawCase c = ShapeFirst(PlanarLL2Text(), 1);
	const std::vector<PlanarFaceWalls> face_walls = PlanarShapeFaceWalls(c.v);
	const PlanarFaceWalls *face = nullptr;
	for (const PlanarFaceWalls &f : face_walls) {
		if (f.face == 33) face = &f;
	}
	REQUIRE(face != nullptr);
	const size_t n = face->walls.size();
	const auto base_options = [&]() {
		PlanarDrawOptions o;
		o.clearance = true;
		o.clear_rows = true;
		o.pairwise = true;
		o.pairwise_all_faces = true;
		o.only_faces = {33};
		o.corridor = true;
		o.tidy = false;
		o.continuous = {"point", "axis", "diagonal", "param", "cross"};
		o.coord_max = 256;
		return o;
	};
	const auto solve = [&](const PlanarDrawOptions &o, const std::string &label) {
		const PlanarDrawResult r = CheckPlanarDrawable(c.v, 15.0, o);
		if (r.status != "Optimal" && r.status != "Infeasible") WARN(fmt::format("face33 time limit or other status in {}: {}", label, r.status));
		return r;
	};

	const PlanarDrawResult base = solve(base_options(), "base");
	WARN(fmt::format("face33 base: {} ms {:.0f}", base.status, base.ms));
	if (base.status != "Infeasible") {
		WARN("face33 base is not Infeasible, stopping");
		return;
	}

	for (int k = 0; k < 4; k++) {
		PlanarDrawOptions o = base_options();
		if (k == 0 || k == 3) o.bays = false;
		if (k == 1 || k == 3) o.curves = false;
		if (k == 2 || k == 3) o.corridor = false;
		const PlanarDrawResult r = solve(o, "family");
		WARN(fmt::format("face33 without {}: {} ms {:.0f}", k == 0 ? "bays" : k == 1 ? "curves" : k == 2 ? "corridor" : "all three", r.status, r.ms));
	}

	std::vector<std::string> candidates;
	for (size_t i = 0; i < n; i++) {
		for (size_t j = i + 1; j < n; j++) {
			if (j == i + 1 || (i == 0 && j == n - 1)) continue;
			candidates.push_back(fmt::format("sep 33 {} {}", i, j));
		}
	}
	std::vector<std::string> kept = candidates;
	for (const std::string &p : candidates) {
		PlanarDrawOptions o = base_options();
		for (const std::string &q : candidates) {
			if (std::ranges::find(kept, q) == kept.end() || q == p) o.skip_pairs.push_back(q);
		}
		const PlanarDrawResult r = solve(o, p);
		if (r.status == "Infeasible") kept.erase(std::ranges::find(kept, p));
	}
	WARN(fmt::format("face33 core size {}", kept.size()));
	for (const std::string &p : kept) {
		std::istringstream words(p);
		std::string family;
		uint32_t f = 0, i = 0, j = 0;
		words >> family >> f >> i >> j;
		WARN(fmt::format("face33 core {}: {} | {}", p, CertWallText(c, face->walls[i], nullptr), CertWallText(c, face->walls[j], nullptr)));
	}
	PlanarDrawOptions only = base_options();
	for (const std::string &q : candidates) {
		if (std::ranges::find(kept, q) == kept.end()) only.skip_pairs.push_back(q);
	}
	const PlanarDrawResult r = solve(only, "core only");
	WARN(fmt::format("face33 core only: {} ms {:.0f}", r.status, r.ms));

	/* Which corridor rows the core needs: only the core's two pairs separate. */
	const PlanarTrackMap &map = c.v.structure.map;
	const auto rows_options = [&](const std::vector<std::string> &skip) {
		PlanarDrawOptions o = base_options();
		o.skip_pairs = only.skip_pairs;
		o.skip_rows = skip;
		return o;
	};
	const PlanarDrawResult rows_check = solve(rows_options({}), "rows check");
	WARN(fmt::format("rows check: {}", rows_check.status));
	if (rows_check.status != "Infeasible") return;
	std::vector<uint32_t> besides;
	for (uint32_t b = 0; b < map.tracks.size(); b++) {
		if (map.part_of[b] != UINT32_MAX) continue;
		std::vector<uint32_t> chain;
		if (!ParentChain(map, c.v.structure.sites, b, chain)) continue;
		besides.push_back(b);
	}
	std::set<uint32_t> needed;
	for (const uint32_t b : besides) {
		const std::string &name = map.tracks[b].name;
		const PlanarDrawResult rr = solve(rows_options({"corridor " + name + " "}), "rows without " + name);
		WARN(fmt::format("rows without {}: {}", name, rr.status));
		if (rr.status == "Optimal") needed.insert(b);
	}
	for (const uint32_t x : needed) {
		const std::string &name = map.tracks[x].name;
		std::vector<std::string> cands = {"corridor " + name + " open", "corridor " + name + " close"};
		for (size_t k = 1; k < c.v.tracks[x].pieces.size(); k++) cands.push_back(fmt::format("corridor {} {}", name, k));
		std::vector<std::string> others;
		for (const uint32_t b : besides) {
			if (b != x && !needed.contains(b)) others.push_back("corridor " + map.tracks[b].name + " ");
		}
		const auto skips_for = [&](const std::vector<std::string> &in_set, const std::string &extra) {
			std::vector<std::string> skip = others;
			for (const std::string &q : cands) {
				if (std::ranges::find(in_set, q) == in_set.end() || q == extra) skip.push_back(q);
			}
			return skip;
		};
		std::vector<std::string> core_rows = cands;
		for (const std::string &p : cands) {
			const PlanarDrawResult rr = solve(rows_options(skips_for(core_rows, p)), "rows core " + name + " " + p);
			if (rr.status == "Infeasible") core_rows.erase(std::ranges::find(core_rows, p));
		}
		std::string text;
		for (const std::string &q : core_rows) text += (text.empty() ? "" : ", ") + q;
		const PlanarDrawResult only_core = solve(rows_options(skips_for(core_rows, "")), "rows core only " + name);
		WARN(fmt::format("rows core {}: {} (core-only check {})", name, text, only_core.status));

		std::vector<uint32_t> chain;
		REQUIRE(ParentChain(map, c.v.structure.sites, x, chain));
		std::string pieces_text, chain_text;
		for (const int h : c.v.tracks[x].pieces) pieces_text += fmt::format("{}{}", pieces_text.empty() ? "" : " ", ((h % 8) + 8) % 8);
		for (const uint32_t t : chain) chain_text += fmt::format("{}{}", chain_text.empty() ? "" : " ", map.tracks[t].name);
		const int side = map.Side(x, 0, chain[0], 0);
		for (const std::string &q : core_rows) {
			std::string line = fmt::format("row {}: beside track {} pieces [{}] parent chain [{}] side {}", q, name, pieces_text, chain_text, side);
			const auto end_text = [&](const PlanarEnd &e) { return fmt::format("{} {}", e.port ? "port" : "node", e.index); };
			if (q.ends_with(" open")) {
				const std::array<int, 2> v = PlanarBesideOpening(c.v.tracks[x].pieces.front(), side);
				line += fmt::format(" opens at {} opening ({},{})", end_text(map.expanded.lines[x].from), v[0], v[1]);
			} else if (q.ends_with(" close")) {
				const std::array<int, 2> v = PlanarBesideClosing(c.v.tracks[x].pieces.back(), side);
				line += fmt::format(" closes at {} closing ({},{})", end_text(map.expanded.lines[x].to), v[0], v[1]);
			}
			WARN(line);
		}
	}
}

TEST_CASE("planar draw: T1 keep-apart conflict read-out", "[.][planart1sep]")
{
	const DrawCase c = ShapeFirst(PlanarTText(1), 1);
	const PlanarTrackMap &map = c.v.structure.map;
	const auto track_name = [&](uint32_t t) { return t == UINT32_MAX ? std::string("box") : t == PLANAR_DUMMY_TRACK ? std::string("dummy") : map.tracks[t].name; };
	for (uint32_t t = 0; t < map.tracks.size(); t++) {
		const auto &line = map.expanded.lines[t];
		std::string text = fmt::format("track {} {}: from {} {} to {} {}; pieces", t, map.tracks[t].name, line.from.port ? "port" : "node", line.from.index, line.to.port ? "port" : "node", line.to.index);
		for (const int h : c.v.tracks[t].pieces) text += fmt::format(" {}", h);
		text += "; flex";
		for (const uint32_t m : c.v.tracks[t].flex) text += fmt::format(" {}", m);
		WARN(text);
	}
	for (const PlanarFaceWalls &f : PlanarShapeFaceWalls(c.v)) {
		std::string text = fmt::format("face {}:", f.face);
		for (size_t i = 0; i < f.walls.size(); i++) text += fmt::format(" [{} {}#{} h{}]", i, track_name(f.walls[i].track), f.walls[i].piece, f.walls[i].heading);
		WARN(text);
	}
	const PlanarDrawPipelineResult r = DrawPlanarPipeline(c.v, {});
	for (const std::string &line : r.log) WARN(line);
	WARN(fmt::format("stopped '{}', final {}", r.stopped, r.final.status));
	for (const std::string &tag : r.final.conflict) WARN("conflict " + tag);
	const auto points_of = [](const std::vector<std::array<int, 2>> &pts) {
		std::string s;
		for (size_t i = 0; i < pts.size(); i++) s += fmt::format(" {}:({},{})", i, pts[i][0], pts[i][1]);
		return s;
	};
	for (const uint32_t t : {7U, 12U, 14U}) {
		if (t < r.last_input.size()) WARN(fmt::format("input {} {}:{}", t, map.tracks[t].name, points_of(r.last_input[t])));
		if (t < r.last_round.points.size()) WARN(fmt::format("round {} {}:{}", t, map.tracks[t].name, points_of(r.last_round.points[t])));
		if (t < r.last_round.lengths.size()) {
			std::string s;
			for (size_t i = 0; i < r.last_round.lengths[t].size(); i++) s += fmt::format(" {}:{:.2f}", i, r.last_round.lengths[t][i]);
			WARN(fmt::format("lengths {} {}:{}", t, map.tracks[t].name, s));
		}
	}
	WARN(fmt::format("last round slack {:.3f}", r.last_round.slack));
	for (const auto &[tag, amount] : r.last_round.stretched) WARN(fmt::format("stretched {} {:.3f}", tag, amount));
}

TEST_CASE("planar draw: elastic mode names a short curve", "[junction][planar][planardraw]")
{
	const DrawCase c = ShapeFirst(PlanarTText(1), 1);
	const PlanarDrawResult plain = CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .curves = false});
	REQUIRE(plain.feasible);
	const PlanarDrawResult result = CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .curves = true, .elastic = true, .elastic_families = {"curve"}, .fix_points = &plain.points, .fix_window = 0});
	CHECK(std::ranges::any_of(result.stretched, [](const auto &s) { return s.first.starts_with("curve "); }));
}

TEST_CASE("planar draw: a prio row runs one lane beside its parent (corridor)", "[junction][planar][planardraw]")
{
	for (const uint32_t lanes : {1U, 2U}) {
		const DrawCase c = ShapeFirst(PlanarTText(lanes), 1);
		const PlanarDrawResult r = CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .corridor = true});
		std::string conflicts;
		for (const std::string &tag : r.conflict) conflicts += tag + "; ";
		INFO("T" << lanes << " status " << r.status << " conflict " << conflicts);
		REQUIRE(r.feasible);
		const PlanarTrackMap &map = c.v.structure.map;
		for (uint32_t b = 0; b < map.tracks.size(); b++) {
			if (map.part_of[b] != UINT32_MAX) continue;
			std::vector<uint32_t> chain;
			REQUIRE(ParentChain(map, c.v.structure.sites, b, chain));
			const int s = map.Side(b, 0, chain[0], 0);
			REQUIRE(s != 0);
			INFO("T" << lanes << " beside " << map.tracks[b].name);
			const std::vector<std::array<int, 2>> &pb = r.points[b];
			const std::vector<int> &pieces = c.v.tracks[b].pieces;
			const int h0 = ((pieces.front() % 8) + 8) % 8, hn = ((pieces.back() % 8) + 8) % 8;
			const std::array<int, 2> open = PlanarBesideOpening(h0, s), close = PlanarBesideClosing(hn, s);
			const std::array<int, 2> first = r.points[chain.front()].front();
			CHECK(pb.front() == std::array<int, 2>{first[0] + open[0], first[1] + open[1]});
			const std::array<int, 2> last = r.points[chain.back()].back();
			CHECK(last == std::array<int, 2>{pb.back()[0] + close[0], pb.back()[1] + close[1]});
			std::vector<std::array<int, 2>> cp;
			for (const uint32_t t : chain) cp.insert(cp.end(), r.points[t].begin() + (cp.empty() ? 0 : 1), r.points[t].end());
			REQUIRE(cp.size() == pb.size());
			for (size_t k = 1; k + 1 < pb.size(); k++) {
				for (const size_t piece : {k - 1, k}) {
					const std::array<int, 2> n = PlanarModel::STEP[(((pieces[piece] % 8) + 8) % 8 + 2) % 8];
					CHECK((pb[k][0] - cp[k][0]) * n[0] + (pb[k][1] - cp[k][1]) * n[1] == 2 * s);
				}
			}
		}
	}
}

TEST_CASE("planar draw: sibling lanes are offset curves on T3", "[junction][planar][planardraw]")
{
	const DrawCase c = ShapeFirst(PlanarTText(3), 1);
	const PlanarDrawResult r = CheckPlanarDrawable(c.v, 5.0, {.clearance = false, .corridor = true, .sibling_lanes = true});
	std::string skipped, conflicts;
	for (const std::string &line : r.lane_skipped) skipped += line + "; ";
	for (const std::string &tag : r.conflict) conflicts += tag + "; ";
	INFO("status " << r.status << " conflict " << conflicts << " skipped " << skipped);
	REQUIRE(r.status == "Optimal");
	REQUIRE(r.lane_pairs >= 1);
	const PlanarTrackMap &map = c.v.structure.map;
	const auto find = [&](std::string_view name) {
		for (uint32_t t = 0; t < map.tracks.size(); t++) {
			if (map.tracks[t].name == name) return t;
		}
		FAIL("no track " << name);
		return UINT32_MAX;
	};
	const uint32_t a = find("D2"), b = find("D3");
	for (const auto &[ch, pa, pb, fa, fb, same] : PlanarSpinePairs(map, c.v.structure, true)) {
		if (pa != a || pb != b) continue;
		REQUIRE(same);
		REQUIRE(fa.size() >= 2);
		const int s = map.Side(b, fb.front() + 1, a, fa.front() + 1);
		REQUIRE(s != 0);
		const uint32_t pa0 = c.v.tracks[a].event_piece[fa.front()], pa1 = c.v.tracks[a].event_piece[fa.back()];
		const uint32_t pb0 = c.v.tracks[b].event_piece[fb.front()];
		for (uint32_t i = 1; i <= pa1 - pa0; i++) {
			for (const uint32_t piece : {pa0 + i - 1, pa0 + i}) {
				const std::array<int, 2> n = PlanarModel::STEP[(((c.v.tracks[a].pieces[piece] % 8) + 8) % 8 + 2) % 8];
				const std::array<int, 2> d = {r.points[b][pb0 + i][0] - r.points[a][pa0 + i][0], r.points[b][pb0 + i][1] - r.points[a][pa0 + i][1]};
				INFO("vertex " << i << " piece " << piece);
				CHECK(d[0] * n[0] + d[1] * n[1] == 2 * s);
			}
		}
		return;
	}
	FAIL("D2/D3 is not a spine pair");
}

TEST_CASE("planar draw: the opening and closing steps are one lane", "[junction][planar][planardraw]")
{
	CHECK(PlanarBesideOpening(2, 1) == std::array<int, 2>{2, 2});
	CHECK(PlanarBesideOpening(5, -1) == std::array<int, 2>{0, 2});
	for (int h = 0; h < 8; h++) {
		for (const int s : {1, -1}) {
			INFO("heading " << h << " side " << s);
			const std::array<int, 2> n = PlanarModel::STEP[(h + 2) % 8];
			const std::array<int, 2> open = PlanarBesideOpening(h, s), close = PlanarBesideClosing(h, s);
			CHECK(open[0] * n[0] + open[1] * n[1] == 2 * s);
			CHECK(close[0] * n[0] + close[1] * n[1] == -2 * s);
			for (const std::array<int, 2> &v : {open, close}) {
				int heading = -1;
				for (int d = 0; d < 8; d++) {
					if (v[0] == 2 * PlanarModel::STEP[d][0] && v[1] == 2 * PlanarModel::STEP[d][1]) heading = d;
				}
				REQUIRE(heading >= 0);
				CHECK(std::abs(PlanarModel::Rot(h, heading)) == 1);
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
static bool LoadPinned197(PlanarShapeVariant &shaped, PlanarDrawPins &pins, bool flex_ends = false)
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
	REQUIRE(PlacePlanarShapeFlex(shaped, why, flex_ends));
	if (!RefinePlanarShapeCells(shaped, why)) {
		WARN(fmt::format("cells not refined: {}", why));
		return false;
	}
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

/** WARN the stretched `sep` rows of a run summed per pair of tracks, the 30 largest. */
static void WarnSepGroups(const PlanarShapeVariant &shaped, const PlanarDrawResult &run, const std::string &tag)
{
	const PlanarTrackMap &map = shaped.structure.map;
	const std::vector<PlanarFaceWalls> faces = PlanarShapeFaceWalls(shaped);
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
}

/** WARN a run's largest stretched `sep` rows with their walls, then its track-pair groups. */
static void WarnSepDetails(const PlanarShapeVariant &shaped, const PlanarDrawResult &run, const std::string &tag)
{
	WarnSepWalls(shaped, run, tag, 40);
	WarnSepGroups(shaped, run, tag);
}

/**
 * The sep-only probe's re-pick loop: sides from the cell-mode elastic drawing at window 8, then each sep-only run re-picks the sides from the run before, to a fixed point (at most 8 runs at window 8, else 2).
 * @return The last Optimal run, or the failing run when none was Optimal.
 */
static PlanarDrawResult SepOnlyFixedPoint(const PlanarShapeVariant &shaped, const PlanarDrawPins &pins, int window, const std::string &label)
{
	PlanarDrawPins p8 = pins;
	p8.window = 8;
	PlanarDrawPins pw = pins;
	pw.window = window;
	const PlanarDrawResult ref0 = CheckPlanarDrawable(shaped, 60.0, {.elastic = true, .pins = &p8});
	WARN(fmt::format("{} reference 0 (cells, W8): {}, slack {:.3f}, {:.0f} ms", label, ref0.status, ref0.slack, ref0.ms));
	if (!ref0.feasible) return ref0;
	const std::vector<std::vector<std::array<int, 2>>> *sides = &ref0.points;
	const int max_runs = window == 8 ? 8 : 2;
	std::vector<PlanarDrawResult> runs;
	runs.reserve(max_runs);
	for (int n = 1; n <= max_runs; n++) {
		runs.push_back(CheckPlanarDrawable(shaped, 60.0, {.clearance = true, .clear_rows = true, .pairwise = true, .elastic = true, .elastic_families = {"sep"}, .pins = &pw, .sides_from = sides}));
		const PlanarDrawResult &run = runs.back();
		const std::string tag = fmt::format("{} W{} run {}", label, window, n);
		WARN(fmt::format("{}: status {}, total slack {:.3f}, stretched rows {}, short_sides {}, {:.0f} ms", tag, run.status, run.slack, run.stretched.size(), run.short_sides, run.ms));
		if (run.status != "Optimal") {
			std::string conflicts;
			for (const std::string &c : run.conflict) conflicts += c + "; ";
			WARN(fmt::format("{} conflict: {}", tag, conflicts));
		}
		if (window != 8) WarnSepDetails(shaped, run, tag);
		if (!run.feasible) break;
		if (n > 1 && runs[n - 2].slack - run.slack <= 0.5) break;
		sides = &run.points;
	}
	for (size_t i = runs.size(); i-- > 0;) {
		if (runs[i].status == "Optimal") return runs[i];
	}
	return runs.back();
}

TEST_CASE("planar draw: PSG 197 sep-only slack, sides from the pinned drawing", "[.][planarkeepapart]")
{
	PlanarShapeVariant shaped;
	PlanarDrawPins pins;
	if (!LoadPinned197(shaped, pins)) return;
	for (const int window : {8, 4}) {
		const PlanarDrawResult last = SepOnlyFixedPoint(shaped, pins, window, "sep-only");
		if (window == 8 && last.status == "Optimal") WarnSepDetails(shaped, last, "sep-only W8 run");
	}
}

TEST_CASE("planar draw: PSG 197 flex on end pieces (option C)", "[.][planarkeepapart]")
{
	const std::vector<std::string> all = {"point", "axis", "diagonal", "param", "cross", "other"};
	for (const bool flex_ends : {false, true}) {
		const std::string label = flex_ends ? "C" : "D402";
		PlanarShapeVariant shaped;
		PlanarDrawPins pins;
		const bool loaded = LoadPinned197(shaped, pins, flex_ends);
		size_t flex = 0;
		for (const PlanarShapeTrack &t : shaped.tracks) flex += t.flex.size();
		WARN(fmt::format("{} flex marks {}, dummies {}, nonmonotone dummies {}", label, flex, shaped.dummies.size(), shaped.nonmonotone_dummies));
		if (!loaded) continue;
		PlanarDrawPins p8 = pins;
		p8.window = 8;
		PlanarDrawPins p4 = pins;
		p4.window = 4;
		for (const PlanarDrawPins *pw : {&p8, &p4}) {
			const PlanarDrawResult lp = CheckPlanarDrawable(shaped, 30.0, {.clearance = false, .pins = pw, .continuous = all});
			WARN(fmt::format("{} base W{} LP: status {}", label, pw->window, lp.status));
			const PlanarDrawResult ip = CheckPlanarDrawable(shaped, 30.0, {.clearance = false, .pins = pw});
			WARN(fmt::format("{} base W{} integer: status {}, {:.0f} ms", label, pw->window, ip.status, ip.ms));
		}
		const PlanarDrawResult cells = CheckPlanarDrawable(shaped, 60.0, {.elastic = true, .pins = &p8});
		WARN(fmt::format("{} cells elastic W8: status {}, total slack {:.3f}, stretched {}", label, cells.status, cells.slack, cells.stretched.size()));
		std::map<std::string, std::pair<size_t, double>> families;
		for (const auto &[tag, slack] : cells.stretched) {
			auto &f = families[tag.substr(0, tag.find(' '))];
			f.first++;
			f.second += slack;
		}
		for (const auto &[family, f] : families) WARN(fmt::format("{} cells elastic W8 family {}: count {}, sum {:.3f}", label, family, f.first, f.second));
		for (const int window : {8, 4}) {
			const PlanarDrawResult last = SepOnlyFixedPoint(shaped, pins, window, label);
			WARN(fmt::format("{} last W{}: status {}, total slack {:.3f}, stretched rows {}", label, window, last.status, last.slack, last.stretched.size()));
			WarnSepGroups(shaped, last, fmt::format("{} last W{}", label, window));
		}
	}
}

TEST_CASE("planar draw: PSG 197 flex-incident pairs (D416 gate)", "[.][planarkeepapart]")
{
	for (const bool flex_ends : {false, true}) {
		const std::string label = flex_ends ? "C" : "D402";
		PlanarShapeVariant shaped;
		PlanarDrawPins pins;
		if (!LoadPinned197(shaped, pins, flex_ends)) continue;
		PlanarDrawPins p8 = pins;
		p8.window = 8;
		const PlanarDrawResult all = CheckPlanarDrawable(shaped, 1.0, {.clearance = true, .clear_rows = true, .pairwise = true, .pins = &p8});
		WARN(fmt::format("{} pairwise all: status {}, pairs {}, binaries {}", label, all.status, all.pairs, all.binaries));
		const PlanarDrawResult only = CheckPlanarDrawable(shaped, 1.0, {.clearance = true, .clear_rows = true, .pairwise = true, .pairwise_flex_only = true, .pins = &p8});
		WARN(fmt::format("{} pairwise flex-only: status {}, pairs {}, binaries {}", label, only.status, only.pairs, only.binaries));
		const PlanarDrawResult solve = CheckPlanarDrawable(shaped, 120.0, {.clearance = true, .clear_rows = true, .pairwise = true, .pairwise_flex_only = true, .pins = &p8});
		WARN(fmt::format("{} pairwise flex-only integer 120 s: status {}, {:.0f} ms, pairs {}, binaries {}", label, solve.status, solve.ms, solve.pairs, solve.binaries));
	}
}

TEST_CASE("planar draw: PSG 197 cell slack by flex role", "[.][planarkeepapart]")
{
	for (const bool flex_ends : {false, true}) {
		const std::string label = flex_ends ? "C" : "D402";
		PlanarShapeVariant shaped;
		PlanarDrawPins pins;
		if (!LoadPinned197(shaped, pins, flex_ends)) continue;
		PlanarDrawPins p8 = pins;
		p8.window = 8;
		const PlanarDrawResult r = CheckPlanarDrawable(shaped, 60.0, {.elastic = true, .pins = &p8});
		WARN(fmt::format("{} cell slack W8: status {}, total slack {:.3f}, stretched {}", label, r.status, r.slack, r.stretched.size()));
		const std::vector<std::vector<PlanarCellWall>> cells = PlanarShapeCellWalls(shaped);
		const auto is_insert = [&](const PlanarCellWall &w) {
			return w.track < shaped.tracks.size() && std::ranges::find(shaped.tracks[w.track].flex, w.piece) != shaped.tracks[w.track].flex.end();
		};
		const auto near_insert = [&](const PlanarCellWall &w) {
			if (w.track >= shaped.tracks.size()) return false;
			const auto &f = shaped.tracks[w.track].flex;
			return std::ranges::find(f, w.piece + 1) != f.end() || (w.piece > 0 && std::ranges::find(f, w.piece - 1) != f.end());
		};
		const auto wall_text = [&](const PlanarCellWall &w) {
			return fmt::format("t{} p{} h{} {}", w.track, w.piece, w.heading, is_insert(w) ? "ins" : "no");
		};
		std::map<std::string, std::pair<size_t, double>> classes, rooms, families;
		std::vector<std::string> largest;
		size_t unmapped = 0;
		for (const auto &[tag, slack] : r.stretched) {
			auto &fam = families[tag.substr(0, tag.find(' '))];
			fam.first++;
			fam.second += slack;
			if (tag.starts_with("room ")) {
				rooms["room"].first++;
				rooms["room"].second += slack;
				continue;
			}
			if (!tag.starts_with("clear ")) continue;
			unsigned idx[3] = {0, 0, 0};
			bool parsed = true;
			const char *p = tag.data() + 6, *end = tag.data() + tag.size();
			for (int i = 0; i < 3 && parsed; i++) {
				const auto res = std::from_chars(p, end, idx[i]);
				parsed = res.ec == std::errc();
				p = res.ptr;
				if (i < 2 && p < end && *p == ' ') p++;
			}
			const unsigned c = idx[0], k = idx[1], e = idx[2];
			if (!parsed || c >= cells.size() || k >= cells[c].size() || e >= cells[c].size()) {
				unmapped++;
				continue;
			}
			const auto &cell = cells[c];
			const size_t n = cell.size();
			const PlanarCellWall &in = cell[k], &out = cell[(k + 1) % n], &target = cell[e];
			const bool a = is_insert(in), b = is_insert(out), t = near_insert(target);
			auto &cls = classes[fmt::format("in-insert {} out-insert {} target-near-insert {}", a ? "Y" : "N", b ? "Y" : "N", t ? "Y" : "N")];
			cls.first++;
			cls.second += slack;
			if (largest.size() < 25) largest.push_back(fmt::format("{} slack {:.3f}: in [{}] out [{}] target [{}] near-insert {}", tag, slack, wall_text(in), wall_text(out), wall_text(target), t ? "yes" : "no"));
		}
		for (const auto &[family, f] : families) WARN(fmt::format("{} family {}: count {}, sum {:.3f}", label, family, f.first, f.second));
		WARN(fmt::format("{} unmapped clear tags {}", label, unmapped));
		for (const auto &[cls, f] : classes) WARN(fmt::format("{} clear class {}: count {}, sum {:.3f}", label, cls, f.first, f.second));
		for (const auto &[cls, f] : rooms) WARN(fmt::format("{} {} rows: count {}, sum {:.3f}", label, cls, f.first, f.second));
		for (const std::string &s : largest) WARN(fmt::format("{} largest {}", label, s));
	}
}

/** Per used step of the extractor's steps.txt: its line, stretch, unflexed base heading (mod 8) and side. */
struct UsedStepA {
	std::string line;
	size_t stretch;
	int base;
	int side;
	bool operator<(const UsedStepA &o) const { return std::tie(line, stretch, base, side) < std::tie(o.line, o.stretch, o.base, o.side); }
};

/** Option A (D417 probe): the flex middles in `used` per track become ordinary pieces listed in `steps`, every other flex pair is erased, then the cells are refined. */
static void ApplyStepsAsBends(PlanarShapeVariant &v, const std::vector<std::set<uint32_t>> &used)
{
	for (size_t t = 0; t < v.tracks.size(); t++) {
		PlanarShapeTrack &track = v.tracks[t];
		std::vector<uint32_t> middles = track.flex;
		std::ranges::sort(middles, std::greater<>());
		for (const uint32_t m : middles) {
			if (used[t].contains(m)) continue;
			track.pieces.erase(track.pieces.begin() + m, track.pieces.begin() + m + 2);
			for (uint32_t &e : track.event_piece) {
				if (e > m) e -= 2;
			}
		}
		std::ranges::sort(middles);
		track.steps.clear();
		uint32_t removed = 0;
		for (const uint32_t m : middles) {
			if (used[t].contains(m)) {
				track.steps.push_back(m - 2 * removed);
			} else {
				removed++;
			}
		}
		track.flex.clear();
	}
}

/** Option A (D417 probe): V's steps from steps.txt applied as bends to Shaped (a LoadPinned197(..., true) variant) and its cells refined; false when they do not refine. */
static bool BuildOptionA(PlanarShapeVariant &shaped)
{
	const PlanarTrackMap &map = shaped.structure.map;

	/* The unflexed shape from shape.txt: per line the headings and the event pieces. */
	std::map<std::string, std::vector<int>> raw_headings;
	std::map<std::string, std::vector<uint32_t>> raw_events;
	{
		std::istringstream in(ReadExtractorFileForDraw("shape.txt"));
		std::string text;
		while (std::getline(in, text)) {
			if (text.starts_with("shape ")) {
				const size_t colon = text.find(':');
				std::istringstream hs(text.substr(colon + 1));
				int h;
				while (hs >> h) raw_headings[text.substr(6, colon - 6)].push_back(h);
			} else if (text.starts_with("at ")) {
				const size_t colon = text.find(':');
				std::string rest = text.substr(colon + 1);
				std::vector<uint32_t> &events = raw_events[text.substr(3, colon - 3)];
				for (size_t at = rest.find('@'); at != std::string::npos; at = rest.find('@', at + 1)) { uint32_t piece = 0; std::from_chars(rest.data() + at + 1, rest.data() + rest.size(), piece); events.push_back(piece); }
			}
		}
	}
	const auto base_headings = [&](const std::string &line, size_t k) {
		std::set<int> bases;
		const std::vector<int> &h = raw_headings[line];
		const std::vector<uint32_t> &ev = raw_events[line];
		if (h.empty() || k > ev.size()) return bases;
		const size_t first = k == 0 ? 0 : ev[k - 1], last = k == ev.size() ? h.size() - 1 : ev[k];
		for (size_t i = first; i <= last && i < h.size(); i++) bases.insert(((h[i] % 8) + 8) % 8);
		return bases;
	};

	std::set<UsedStepA> steps;
	size_t parsed = 0;
	{
		std::istringstream in(ReadExtractorFileForDraw("steps.txt"));
		std::string text;
		while (std::getline(in, text)) {
			if (!text.starts_with("step ")) continue;
			parsed++;
			const size_t hash = text.find(" # ");
			std::istringstream ls(text.substr(5, hash == std::string::npos ? std::string::npos : hash - 5));
			std::string name;
			size_t k;
			char colon;
			ls >> name >> k >> colon;
			std::vector<int> h;
			int x;
			while (ls >> x) h.push_back(x);
			const std::set<int> bases = base_headings(name, k);
			for (size_t i = 1; i + 1 < h.size(); i++) {
				if (h[i - 1] != h[i + 1]) continue;
				const int d = (((h[i] - h[i - 1]) % 8) + 8) % 8;
				if (d != 1 && d != 7) continue;
				const int a = ((h[i - 1] % 8) + 8) % 8;
				if (!bases.contains(a)) continue;
				steps.insert({name, k, a, d == 1 ? 1 : -1});
			}
		}
	}
	std::vector<std::set<uint32_t>> used(shaped.tracks.size());
	size_t matched = 0, unmatched = 0;
	for (const UsedStepA &s : steps) {
		uint32_t t = UINT32_MAX;
		for (uint32_t i = 0; i < map.tracks.size() && t == UINT32_MAX; i++) {
			if (map.tracks[i].kind == PlanarTrack::Kind::Line && map.tracks[i].name == s.line) t = i;
		}
		bool found = false;
		if (t != UINT32_MAX && s.stretch <= shaped.tracks[t].event_piece.size()) {
			const PlanarShapeTrack &track = shaped.tracks[t];
			const int64_t first = s.stretch == 0 ? -1 : static_cast<int64_t>(track.event_piece[s.stretch - 1]);
			const int64_t last = s.stretch == track.event_piece.size() ? static_cast<int64_t>(track.pieces.size() - 1) : static_cast<int64_t>(track.event_piece[s.stretch]);
			for (const uint32_t m : track.flex) {
				if (static_cast<int64_t>(m) > first && static_cast<int64_t>(m) < last && ((track.pieces[m - 1] % 8) + 8) % 8 == s.base && track.pieces[m] - track.pieces[m - 1] == s.side) {
					used[t].insert(m);
					found = true;
					break;
				}
			}
		}
		if (found) {
			matched++;
		} else {
			unmatched++;
			WARN(fmt::format("A unmatched step {} {} base {} side {}", s.line, s.stretch, s.base, s.side));
		}
	}
	WARN(fmt::format("A used steps found {}, matched {}, unmatched {}, total steps parsed {}, pieces added by used steps {}", steps.size(), matched, unmatched, parsed, matched * 2));
	ApplyStepsAsBends(shaped, used);
	std::string why;
	if (!RefinePlanarShapeCells(shaped, why)) {
		WARN(fmt::format("A cells not refined: {}", why));
		return false;
	}
	WARN(fmt::format("A dummies after {}, nonmonotone {}", shaped.dummies.size(), shaped.nonmonotone_dummies));
	return true;
}

TEST_CASE("planar draw: PSG 197 option A, V's steps as bends", "[.][planarkeepapart]")
{
	PlanarShapeVariant d402;
	PlanarDrawPins d402_pins;
	if (!LoadPinned197(d402, d402_pins, false)) return;
	PlanarShapeVariant shaped;
	PlanarDrawPins pins;
	if (!LoadPinned197(shaped, pins, true)) return;
	WARN(fmt::format("A dummies D402 {}, nonmonotone {}", d402.dummies.size(), d402.nonmonotone_dummies));
	if (!BuildOptionA(shaped)) return;

	const auto run_cells = [&](const PlanarShapeVariant &shaped, const char *label) {
		for (const int window : {8, 4, 2}) {
			PlanarDrawPins pw = pins;
			pw.window = window;
			const PlanarDrawResult base = CheckPlanarDrawable(shaped, 60.0, {.clearance = false, .elastic = true, .pins = &pw});
			WARN(fmt::format("{} base W{} elastic: status {}, total slack {:.3f}, stretched rows {}", label, window, base.status, base.slack, base.stretched.size()));
			for (const auto &[tag, slack] : base.stretched) WARN(fmt::format("{} base W{} row {}: {:.3f}", label, window, tag, slack));
			const PlanarDrawResult cells = CheckPlanarDrawable(shaped, 60.0, {.elastic = true, .pins = &pw});
			WARN(fmt::format("{} cells W{} elastic: status {}, total slack {:.3f}, {:.0f} ms, stretched rows {}", label, window, cells.status, cells.slack, cells.ms, cells.stretched.size()));
			std::map<std::string, std::pair<size_t, double>> families;
			for (const auto &[tag, slack] : cells.stretched) {
				auto &f = families[tag.substr(0, tag.find(' '))];
				f.first++;
				f.second += slack;
			}
			for (const auto &[family, f] : families) WARN(fmt::format("{} cells W{} family {}: count {}, sum {:.3f}", label, window, family, f.first, f.second));
			for (size_t i = 0; i < cells.stretched.size() && i < 20; i++) WARN(fmt::format("{} cells W{} top {}: {} {:.3f}", label, window, i + 1, cells.stretched[i].first, cells.stretched[i].second));
			if (window == 8) {
				/* Whether dummy d starts at a step bend: its piece is the flank before the insert or the insert itself. */
				const auto step_dummy = [&](uint32_t d) {
					const PlanarShapeDummy &dm = shaped.dummies[d];
					return dm.track < shaped.tracks.size() && std::ranges::any_of(shaped.tracks[dm.track].steps, [&](uint32_t m) { return dm.piece + 1 == m || dm.piece == m; });
				};
				const std::vector<std::vector<PlanarCellWall>> cell_walls = PlanarShapeCellWalls(shaped);
				std::map<std::string, std::pair<size_t, double>> classes;
				for (const auto &[tag, slack] : cells.stretched) {
					const bool clear = tag.starts_with("clear ");
					if (!clear && !tag.starts_with("land ")) continue;
					bool step = false;
					if (clear) {
						size_t cell = 0;
						std::from_chars(tag.data() + 6, tag.data() + tag.size(), cell);
						if (cell < cell_walls.size()) {
							for (const PlanarCellWall &w : cell_walls[cell]) {
								if (w.track == PLANAR_DUMMY_TRACK && w.piece < shaped.dummies.size() && step_dummy(w.piece)) step = true;
							}
						}
					} else {
						size_t d = 0;
						std::from_chars(tag.data() + 5, tag.data() + tag.size(), d);
						step = d < shaped.dummies.size() && step_dummy(static_cast<uint32_t>(d));
					}
					auto &c = classes[std::string(clear ? "clear" : "land") + (step ? " step-dummy cell" : " other")];
					c.first++;
					c.second += slack;
				}
				for (const auto &[name, c] : classes) WARN(fmt::format("{} cells W8 class {}: count {}, sum {:.3f}", label, name, c.first, c.second));
			}
			if (window == 8) {
				const PlanarDrawResult ip = CheckPlanarDrawable(shaped, 60.0, {.pins = &pw});
				WARN(fmt::format("{} cells W8 integer: status {}, {:.0f} ms", label, ip.status, ip.ms));
				if (ip.status == "Infeasible") {
					for (size_t i = 0; i < ip.conflict.size() && i < 30; i++) WARN(fmt::format("{} cells W8 conflict {}: {}", label, i + 1, ip.conflict[i]));
				}
			}
		}
	};
	run_cells(shaped, "A");

	/* Refinement again with its landings taken from the drawing of the base run at window 2 (slack 0). */
	PlanarShapeVariant geo = shaped;
	{
		PlanarDrawPins p2 = pins;
		p2.window = 2;
		const PlanarDrawResult ref = CheckPlanarDrawable(shaped, 60.0, {.clearance = false, .elastic = true, .pins = &p2});
		WARN(fmt::format("A-geo reference: status {}, total slack {:.3f}", ref.status, ref.slack));
		std::string geo_why;
		if (!RefinePlanarShapeCells(geo, geo_why, &ref.points)) {
			WARN(fmt::format("A-geo cells not refined: {}", geo_why));
			return;
		}
		size_t differ = 0;
		for (size_t i = 0; i < geo.dummies.size() && i < shaped.dummies.size(); i++) {
			const PlanarShapeDummy &x = geo.dummies[i], &y = shaped.dummies[i];
			if (std::tie(x.face, x.track, x.piece, x.heading, x.land_track, x.land_piece) != std::tie(y.face, y.track, y.piece, y.heading, y.land_track, y.land_piece)) differ++;
		}
		differ += geo.dummies.size() > shaped.dummies.size() ? geo.dummies.size() - shaped.dummies.size() : shaped.dummies.size() - geo.dummies.size();
		WARN(fmt::format("A-geo dummies {}, nonmonotone {}, landing fallbacks {}, differing from the rule's {}", geo.dummies.size(), geo.nonmonotone_dummies, geo.landing_fallbacks, differ));
	}
	run_cells(geo, "A-geo");
	PlanarDrawPins p8 = d402_pins;
	p8.window = 8;
	const PlanarDrawResult plain = CheckPlanarDrawable(d402, 60.0, {.elastic = true, .pins = &p8});
	WARN(fmt::format("A D402 cells W8 elastic: status {}, total slack {:.3f}", plain.status, plain.slack));
}

TEST_CASE("planar draw: PSG 197 clearance rows evaluated on V's drawing", "[.][planarkeepapart]")
{
	PlanarShapeVariant shaped;
	PlanarDrawPins pins;
	if (!LoadPinned197(shaped, pins, true)) return;
	if (!BuildOptionA(shaped)) return;
	const PlanarTrackMap &map = shaped.structure.map;
	const size_t nt = shaped.tracks.size();

	/* V's real geometry per line: its start pin, then its real piece lengths along the variant's own headings. */
	std::map<std::string, std::vector<int>> lengths;
	{
		std::ifstream in(std::string(JUNCTION_TOPOLOGY_DIR) + "/../corpus/out/psg197/lengths.txt");
		std::string text;
		while (std::getline(in, text)) {
			if (!text.starts_with("lengths ")) continue;
			const size_t colon = text.find(':');
			std::istringstream ls(text.substr(colon + 1));
			std::vector<int> &l = lengths[text.substr(8, colon - 8)];
			int x;
			while (ls >> x) l.push_back(x);
		}
	}
	std::vector<std::vector<std::array<int, 2>>> vref(nt);
	std::map<std::string, size_t> reasons;
	std::map<size_t, std::string> track_reason;
	size_t fixed = 0;
	for (size_t t = 0; t < nt; t++) {
		if (map.tracks[t].kind != PlanarTrack::Kind::Line || !lengths.contains(map.tracks[t].name)) {
			reasons["not a line"]++;
			continue;
		}
		if (map.tracks[t].name == "L89") {
			reasons["left unfixed (V's step has no flex pair)"]++;
			track_reason[t] = "flex";
			continue;
		}
		const std::vector<int> &l = lengths[map.tracks[t].name];
		const std::vector<int> &pieces = shaped.tracks[t].pieces;
		if (l.size() != pieces.size()) {
			reasons["piece count differs"]++;
			track_reason[t] = "piece count";
			continue;
		}
		const PlanarDrawPins::Pin *start = nullptr, *end = nullptr;
		for (const PlanarDrawPins::Pin &pin : pins.pins) {
			if (pin.cross || pin.track != t) continue;
			(pin.start ? start : end) = &pin;
		}
		if (start == nullptr) {
			reasons["no start pin"]++;
			continue;
		}
		std::vector<std::array<int, 2>> points = {start->at};
		for (size_t i = 0; i < l.size(); i++) {
			const std::array<int, 2> step = PlanarModel::STEP[((pieces[i] % 8) + 8) % 8];
			points.push_back({points.back()[0] + l[i] * step[0], points.back()[1] + l[i] * step[1]});
		}
		if (end != nullptr && points.back() != end->at) {
			WARN(fmt::format("end mismatch {}: got ({}, {}), pin ({}, {})", map.tracks[t].name, points.back()[0], points.back()[1], end->at[0], end->at[1]));
			reasons["end mismatch"]++;
			track_reason[t] = "end mismatch";
			continue;
		}
		vref[t] = points;
		fixed++;
	}
	WARN(fmt::format("tracks {}, fixed {}", nt, fixed));
	for (const auto &[reason, count] : reasons) WARN(fmt::format("unfixed: {}: {}", reason, count));

	/* The box's low corner moves to 0. */
	std::array<int, 2> low = {INT_MAX, INT_MAX};
	/* The box's low edges: a W port's end pins hold x, an N port's hold y; a side without a port (PSG 197 has no W port) is the least coordinate of any pin. */
	std::array<bool, 2> edge_port = {false, false};
	for (size_t p = 0; p < map.expanded.ports.size(); p++) {
		const PlanarSide side = map.expanded.ports[p].side;
		if (side != PlanarSide::W && side != PlanarSide::N) continue;
		const int axis = side == PlanarSide::W ? 0 : 1;
		for (const uint32_t line : map.expanded.ports[p].lines) {
			const PlanarEnd &from = map.expanded.lines[line].from;
			const bool at_start = from.port && from.index == p;
			for (const PlanarDrawPins::Pin &pin : pins.pins) {
				if (pin.cross || pin.track != line || pin.start != at_start) continue;
				low[axis] = std::min(low[axis], pin.at[axis]);
				edge_port[axis] = true;
			}
		}
	}
	for (int axis = 0; axis < 2; axis++) {
		if (edge_port[axis]) continue;
		for (const PlanarDrawPins::Pin &pin : pins.pins) low[axis] = std::min(low[axis], pin.at[axis]);
	}
	WARN(fmt::format("box low corner ({}, {})", low[0], low[1]));
	for (auto &track : vref) {
		for (auto &p : track) {
			p[0] -= low[0];
			p[1] -= low[1];
		}
	}
	/* PLANAR_DUMP (a measurement aid): the dumps go to <dir>/197v; vref.json holds V's drawing. */
	std::string dump_dir_197v;
	if (const char *s = std::getenv("PLANAR_DUMP"); s != nullptr) {
		dump_dir_197v = fmt::format("{}/197v", s);
		std::filesystem::create_directories(dump_dir_197v);
		std::ofstream ref(dump_dir_197v + "/vref.json");
		ref << "{\"tracks\": [";
		for (size_t t = 0; t < nt; t++) {
			ref << fmt::format("{}{{\"name\": \"{}\", \"points\": [", t == 0 ? "" : ", ", map.tracks[t].name);
			for (size_t k = 0; k < vref[t].size(); k++) ref << fmt::format("{}[{},{}]", k == 0 ? "" : ",", vref[t][k][0], vref[t][k][1]);
			ref << "]}";
		}
		ref << "]}\n";
	}

	/* V's node legs (read-out): travel between a node and the nearest event, and between two nodes with no event. */
	{
		std::ofstream legs("/home/pete/OpenTTD/.claude/ng1/build-rel/vlegs.txt");
		const auto say = [&](const std::string &line) {
			WARN(line);
			legs << line << "\n";
		};
		const auto travel = [](const std::vector<std::array<int, 2>> &pts, size_t a, size_t b) {
			int sum = 0;
			for (size_t i = a; i < b; i++) sum += std::max(std::abs(pts[i + 1][0] - pts[i][0]), std::abs(pts[i + 1][1] - pts[i][1]));
			return sum;
		};
		std::array<std::array<size_t, 5>, 3> hist{};
		std::array<size_t, 3> total{};
		size_t skipped = 0;
		const auto count = [&](size_t kind, int v) {
			total[kind]++;
			hist[kind][std::min(v, 4)]++;
		};
		for (size_t t = 0; t < nt; t++) {
			const std::vector<std::array<int, 2>> &pts = vref[t];
			const size_t np = shaped.tracks[t].pieces.size();
			if (pts.empty() || pts.size() != np + 1) {
				skipped++;
				continue;
			}
			const std::string &name = map.tracks[t].name;
			const PlanarEnd &from = map.expanded.lines[t].from;
			const PlanarEnd &to = map.expanded.lines[t].to;
			const size_t ne = map.events[t].size();
			const auto &ep = shaped.tracks[t].event_piece;
			const bool has_event = ne > 0 && ep.size() >= ne;
			const int whole = travel(pts, 0, np);
			if (!from.port) {
				const int v = has_event ? travel(pts, 0, ep[0]) : whole;
				say(fmt::format("start leg {} node {}: {} (first event on piece {})", name, from.index, v, has_event ? fmt::format("{}", ep[0]) : std::string("none")));
				count(0, v);
			}
			if (!to.port) {
				const int v = has_event ? travel(pts, ep[ne - 1] + 1, np) : whole;
				say(fmt::format("end leg {} node {}: {} (last event on piece {})", name, to.index, v, has_event ? fmt::format("{}", ep[ne - 1]) : std::string("none")));
				count(1, v);
			}
			if (!from.port && !to.port && !has_event) {
				say(fmt::format("between nodes {} -> {} {}: {}", from.index, to.index, name, whole));
				count(2, whole);
			}
		}
		const char *kinds[3] = {"start legs", "end legs", "between-node tracks"};
		std::string sum = fmt::format("node legs summary (skipped {}):", skipped);
		for (size_t k = 0; k < 3; k++) sum += fmt::format(" {} {} [0:{} 1:{} 2:{} 3:{} >=4:{}];", kinds[k], total[k], hist[k][0], hist[k][1], hist[k][2], hist[k][3], hist[k][4]);
		say(sum);

		std::ofstream tr("/home/pete/OpenTTD/.claude/ng1/build-rel/vtravel.txt");
		for (size_t t = 0; t < nt; t++) {
			const std::vector<std::array<int, 2>> &pts = vref[t];
			if (pts.size() < 2 || pts.size() != shaped.tracks[t].pieces.size() + 1) continue;
			tr << map.tracks[t].name << " " << travel(pts, 0, pts.size() - 1) << " " << pts.size() - 1 << "\n";
		}
	}

	PlanarDrawPins p8 = pins;
	p8.window = 8;
	const auto evaluate = [&](const PlanarShapeVariant &sv, const std::string &name) {
		const std::vector<std::vector<PlanarCellWall>> cells = PlanarShapeCellWalls(sv);
		const auto is_insert = [&](const PlanarCellWall &w) {
			if (w.track >= sv.tracks.size()) return false;
			const PlanarShapeTrack &t = sv.tracks[w.track];
			return std::ranges::find(t.flex, w.piece) != t.flex.end() || std::ranges::find(t.steps, w.piece) != t.steps.end();
		};
		const auto wall_text = [&](const PlanarCellWall &w) {
			if (w.track == PLANAR_DUMMY_TRACK) return fmt::format("dummy p{} h{}", w.piece, w.heading);
			if (w.track >= map.tracks.size()) return fmt::format("edge p{} h{}", w.piece, w.heading);
			return fmt::format("{} p{} h{}{}", map.tracks[w.track].name, w.piece, w.heading, is_insert(w) ? " ins" : "");
		};
		for (const int window : {1, 0}) {
			const std::string label = fmt::format("{} W{}", name, window);
			const PlanarDrawResult r = CheckPlanarDrawable(sv, 60.0, {.elastic = true, .elastic_families = {"clear", "land", "room", "deck", "end", "node", "cross"}, .pins = &p8, .fix_points = &vref, .fix_window = window});
			WARN(fmt::format("{}: status {}, total slack {:.3f}, stretched rows {}", label, r.status, r.slack, r.stretched.size()));
			if (r.status != "Optimal") {
				for (const std::string &tag : r.conflict) WARN(fmt::format("{} conflict {}", label, tag));
				continue;
			}
			std::map<std::string, std::pair<size_t, double>> families;
			for (const auto &[tag, slack] : r.stretched) {
				auto &f = families[tag.substr(0, tag.find(' '))];
				f.first++;
				f.second += slack;
			}
			for (const auto &[family, f] : families) WARN(fmt::format("{} family {}: rows {}, slack {:.3f}", label, family, f.first, f.second));
			if (window != 1) continue;
			for (const auto &[tag, slack] : r.stretched) {
				uint32_t n = UINT32_MAX;
				if (tag.starts_with("cross ")) {
					std::from_chars(tag.data() + 6, tag.data() + tag.size(), n);
					if (n < map.crossings.size()) WARN(fmt::format("{} stretched {} {:.3f}: crossing {} x {}", label, tag, slack, map.tracks[map.crossings[n].a].name, map.tracks[map.crossings[n].b].name));
				} else if (tag.starts_with("deck ") || tag.starts_with("end ")) {
					std::from_chars(tag.data() + tag.find(' ') + 1, tag.data() + tag.size(), n);
					if (n < sv.structure.structures.size()) {
						const PlanarStructure &st = sv.structure.structures[n];
						std::string crossings;
						for (const uint32_t c : st.crossings) crossings += fmt::format(" [{} x {}]", map.tracks[map.crossings[c].a].name, map.tracks[map.crossings[c].b].name);
						WARN(fmt::format("{} stretched {} {:.3f}: structure separator {}, crossings{}", label, tag, slack, map.tracks[st.separator].name, crossings));
					}
				} else if (tag.starts_with("node ")) {
					std::from_chars(tag.data() + 5, tag.data() + tag.size(), n);
					if (n < map.expanded.nodes.size()) {
						std::string lines;
						for (const uint32_t line : map.expanded.nodes[n].in) lines += " in " + map.tracks[line].name;
						for (const uint32_t line : map.expanded.nodes[n].out) lines += " out " + map.tracks[line].name;
						WARN(fmt::format("{} stretched {} {:.3f}: node{}", label, tag, slack, lines));
					}
				}
			}
			if (window != 1) continue;
			std::map<std::string, std::pair<size_t, double>> groups;
			double land = 0, room = 0;
			size_t lands = 0, rooms = 0, unmapped = 0;
			std::vector<std::pair<double, std::string>> worst;
			for (const auto &[tag, slack] : r.stretched) {
				if (tag.starts_with("land ")) {
					lands++;
					land += slack;
					continue;
				}
				if (tag.starts_with("room ")) {
					rooms++;
					room += slack;
					continue;
				}
				if (!tag.starts_with("clear ")) continue;
				unsigned idx[3] = {0, 0, 0};
				bool parsed = true;
				const char *p = tag.data() + 6, *end = tag.data() + tag.size();
				for (int i = 0; i < 3 && parsed; i++) {
					const auto res = std::from_chars(p, end, idx[i]);
					parsed = res.ec == std::errc();
					p = res.ptr;
					if (i < 2 && p < end && *p == ' ') p++;
				}
				const unsigned c = idx[0], k = idx[1], e = idx[2];
				if (!parsed || c >= cells.size() || k >= cells[c].size() || e >= cells[c].size()) {
					unmapped++;
					continue;
				}
				const auto &cell = cells[c];
				const size_t n = cell.size();
				const PlanarCellWall &in = cell[k], &out = cell[(k + 1) % n], &target = cell[e];
				const bool dummy = std::ranges::any_of(cell, [](const PlanarCellWall &w) { return w.track == PLANAR_DUMMY_TRACK; });
				auto &g = groups[fmt::format("target h{} in h{} out h{} insert-vertex {} dummy-cell {}", target.heading % 8, in.heading % 8, out.heading % 8, is_insert(in) || is_insert(out) ? "Y" : "N", dummy ? "Y" : "N")];
				g.first++;
				g.second += slack;
				worst.emplace_back(slack, fmt::format("{} slack {:.3f}: in [{}] out [{}] target [{}]", tag, slack, wall_text(in), wall_text(out), wall_text(target)));
			}
			WARN(fmt::format("{} land rows {} sum {:.3f}, room rows {} sum {:.3f}, clear rows {}, unmapped clear tags {}", label, lands, land, rooms, room, worst.size(), unmapped));
			std::vector<std::pair<std::string, std::pair<size_t, double>>> ordered(groups.begin(), groups.end());
			std::ranges::sort(ordered, [](const auto &x, const auto &y) { return x.second.second > y.second.second; });
			for (size_t i = 0; i < ordered.size() && i < 30; i++) WARN(fmt::format("{} group {}: count {}, sum {:.3f}", label, ordered[i].first, ordered[i].second.first, ordered[i].second.second));
			std::ranges::sort(worst, [](const auto &x, const auto &y) { return x.first > y.first; });
			for (size_t i = 0; i < worst.size() && i < 25; i++) WARN(fmt::format("{} worst {}: {}", label, i + 1, worst[i].second));
		}
	};
	/* PLANAR_PROBE_BAYS_ON_V (a measurement aid, D420): the bay rows on V's drawing (points held to vref, window 1), every family elastic, bay rows reported; then stop. */
	if (std::getenv("PLANAR_PROBE_BAYS_ON_V") != nullptr) {
		const PlanarDrawResult b = CheckPlanarDrawable(shaped, 60.0, {.clearance = false, .bays = true, .curves = std::getenv("PLANAR_PROBE_CURVES_ON_V") != nullptr, .elastic = true, .pins = &p8, .fix_points = &vref, .fix_window = 1, .export_path = std::getenv("PLANAR_PROBE_BAYS_ON_V")});
		WARN(fmt::format("bays on V: status {}, total slack {:.3f}, rules.bay {}", b.status, b.slack, shaped.structure.rules.bay));
		for (const auto &[row, slack] : b.stretched) WARN(fmt::format("bays on V stretched {}: {:.3f}", row, slack));
		return;
	}
	evaluate(shaped, "A real");

	/* Geo: the cells refined again with landings that agree with V's geometry (a base-rows drawing with V's points held where fixed). */
	const PlanarDrawResult full = CheckPlanarDrawable(shaped, 60.0, {.clearance = false, .elastic = true, .pins = &p8, .fix_points = &vref, .fix_window = 1});
	WARN(fmt::format("geo reference: status {}, total slack {:.3f}", full.status, full.slack));
	{
		const PlanarDrawResult g = CheckPlanarDrawable(shaped, 60.0, {.clearance = false, .node_gaps = true, .elastic = true, .pins = &p8, .fix_points = &vref, .fix_window = 1});
		WARN(fmt::format("legs on V: status {}, total slack {:.3f}", g.status, g.slack));
		for (const auto &[row, slack] : g.stretched) {
			if (row.starts_with("leg")) WARN(fmt::format("legs on V stretched {}: {:.3f}", row, slack));
		}
	}

	/* Pairwise separation rows on V's geometry, each pair's side taken from the base-rows reference. */
	for (const int window : {1, 0}) {
		const std::string label = fmt::format("sepV W{}", window);
		const PlanarDrawResult r = CheckPlanarDrawable(shaped, 60.0, {.clearance = true, .clear_rows = true, .pairwise = true, .elastic = true, .elastic_families = {"sep", "clear", "deck", "end", "node", "cross"}, .pins = &p8, .sides_from = &full.points, .fix_points = &vref, .fix_window = window});
		WARN(fmt::format("{}: status {}, total slack {:.3f}, pairs {}, short_sides {}", label, r.status, r.slack, r.pairs, r.short_sides));
		if (r.status != "Optimal") {
			for (const std::string &tag : r.conflict) WARN(fmt::format("{} conflict {}", label, tag));
			continue;
		}
		std::map<std::string, std::pair<size_t, double>> families;
		for (const auto &[tag, slack] : r.stretched) {
			auto &f = families[tag.substr(0, tag.find(' '))];
			f.first++;
			f.second += slack;
		}
		for (const auto &[family, f] : families) WARN(fmt::format("{} family {}: rows {}, slack {:.3f}", label, family, f.first, f.second));
		if (window != 1) continue;
		WarnSepWalls(shaped, r, label, 40);
		WarnSepGroups(shaped, r, label);
		size_t shown = 0;
		for (const auto &[tag, slack] : r.stretched) {
			if (!tag.starts_with("clear ") || shown >= 15) continue;
			shown++;
			WARN(fmt::format("{} clear {}: {} {:.3f}", label, shown, tag, slack));
		}
	}
	/* The V-sides run with convex faces separated pairwise too (no cell clearance rows). */
	{
		const std::string label = "sepV allfaces W1";
		const PlanarDrawResult r = CheckPlanarDrawable(shaped, 60.0, {.clearance = true, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .elastic = true, .elastic_families = {"sep", "clear", "deck", "end", "node", "cross"}, .pins = &p8, .sides_from = &full.points, .fix_points = &vref, .fix_window = 1, .dump_path = dump_dir_197v.empty() ? std::string() : dump_dir_197v + "/sepv-allfaces.json"});
		WARN(fmt::format("{}: status {}, total slack {:.3f}, pairs {}, short_sides {}", label, r.status, r.slack, r.pairs, r.short_sides));
		if (r.status != "Optimal") {
			for (const std::string &tag : r.conflict) WARN(fmt::format("{} conflict {}", label, tag));
		} else {
			std::map<std::string, std::pair<size_t, double>> families;
			for (const auto &[tag, slack] : r.stretched) {
				auto &f = families[tag.substr(0, tag.find(' '))];
				f.first++;
				f.second += slack;
			}
			for (const auto &[family, f] : families) WARN(fmt::format("{} family {}: rows {}, slack {:.3f}", label, family, f.first, f.second));
			WarnSepWalls(shaped, r, label, 20);
		}
	}
	/* A read-out (PLANAR_PROBE_VRES): the allfaces W1 evaluation with the sides from V's own points, window 1 and 0, and every stretched row decoded against V's coordinates. */
	if (std::getenv("PLANAR_PROBE_VRES") != nullptr) {
		std::vector<std::vector<std::array<int, 2>>> hybrid = vref;
		std::string filled;
		for (size_t t = 0; t < nt; t++) {
			if (!vref[t].empty()) continue;
			hybrid[t] = full.points[t];
			filled += fmt::format(" {} ({})", map.tracks[t].name, track_reason.contains(t) ? track_reason[t] : std::string("not a line"));
		}
		WARN(fmt::format("vres sides V+full: tracks filled from full.points:{}", filled));
		const PlanarDrawOptions base_opts = {.clearance = true, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .elastic = true, .elastic_families = {"sep", "clear", "deck", "end", "node", "cross"}, .pins = &p8, .sides_from = &hybrid, .fix_points = &vref, .fix_window = 1};
		PlanarDrawOptions dump_opts = base_opts;
		if (!dump_dir_197v.empty()) dump_opts.dump_path = dump_dir_197v + "/vres.json";
		const PlanarDrawResult r = CheckPlanarDrawable(shaped, 60.0, dump_opts);
		const std::string label = "vres sides V+full";
		WARN(fmt::format("vres sides V+full: status {}, total slack {:.3f}, short_sides {}", r.status, r.slack, r.short_sides));
		PlanarDrawOptions w0 = base_opts;
		w0.fix_window = 0;
		const PlanarDrawResult r0 = CheckPlanarDrawable(shaped, 60.0, w0);
		WARN(fmt::format("vres sides V+full window 0: status {}, total slack {:.3f}", r0.status, r0.slack));
		if (r0.status != "Optimal") {
			for (size_t i = 0; i < std::min<size_t>(40, r0.conflict.size()); i++) WARN(fmt::format("vres sides V+full window 0 conflict {}", r0.conflict[i]));
		}
		if (r.status == "Optimal") {
			std::map<std::string, std::pair<size_t, double>> families;
			for (const auto &[tag, slack] : r.stretched) {
				auto &f = families[tag.substr(0, tag.find(' '))];
				f.first++;
				f.second += slack;
			}
			for (const auto &[family, f] : families) WARN(fmt::format("{} family {}: rows {}, slack {:.3f}", label, family, f.first, f.second));
			const std::vector<PlanarFaceWalls> fw = PlanarShapeFaceWalls(shaped);
			const auto pt = [](const std::array<int, 2> &p) { return fmt::format("({},{})", p[0], p[1]); };
			const auto vpts = [&](size_t t, size_t a, size_t b) {
				std::string s;
				const bool notv = vref[t].empty();
				const auto &src = notv ? full.points[t] : vref[t];
				for (size_t k = a; k <= b; k++) s += (k < src.size() ? pt(src[k]) : std::string("(none)")) + " ";
				return s + (notv ? "(not V) " : "");
			};
			const auto rpts = [&](size_t t, size_t a, size_t b) {
				std::string s;
				if (t >= r.points.size()) return s;
				for (size_t k = a; k <= b; k++) s += (k < r.points[t].size() ? pt(r.points[t][k]) : std::string("(none)")) + " ";
				return s;
			};
			const auto wall_name = [&](const PlanarCellWall &w) { if (w.track >= nt) return fmt::format("dummy#{} h{}", w.piece, w.heading); return fmt::format("{}#{} h{}", map.tracks[w.track].name, w.piece, w.heading); };
			const auto wall_len = [&](const PlanarCellWall &w) {
				if (w.track >= nt) return -1;
				const auto &src = hybrid[w.track];
				if (w.piece + 1 >= src.size()) return -1;
				const auto &a = src[w.piece], &b = src[w.piece + 1];
				return std::max(std::abs(a[0] - b[0]), std::abs(a[1] - b[1]));
			};
			/* Every candidate's margin on V, for all of the largest sep tags in one run. */
			std::vector<std::string> sep_tags;
			for (const auto &[tag, slack] : r.stretched) {
				if (tag.starts_with("sep ") && sep_tags.size() < 30) sep_tags.push_back(tag);
			}
			PlanarDrawOptions mo = base_opts;
			mo.margin_tags = sep_tags;
			const PlanarDrawResult rm = CheckPlanarDrawable(shaped, 60.0, mo);
			std::map<std::string, std::string> margins(rm.sep_margins.begin(), rm.sep_margins.end());
			const auto ends_of = [&](size_t t) { return std::pair{map.expanded.lines[t].from, map.expanded.lines[t].to}; };
			const auto same_end = [](const PlanarEnd &x, const PlanarEnd &y) { return x.port == y.port && x.index == y.index; };
			size_t shown = 0;
			for (const auto &[tag, slack] : r.stretched) {
				if (shown >= 30) break;
				shown++;
				if (tag.starts_with("sep ")) {
					std::istringstream in(tag.substr(4));
					uint32_t face = 0;
					size_t a = 0, b = 0;
					in >> face >> a >> b;
					for (const PlanarFaceWalls &f : fw) {
						if (f.face != face || a >= f.walls.size() || b >= f.walls.size()) continue;
						if (f.walls[a].track >= nt || f.walls[b].track >= nt) {
							WARN(fmt::format("vres row {} ({:.3f}): a wall is a dummy, not decoded", tag, slack));
							continue;
						}
						const PlanarCellWall &wi = f.walls[a], &wj = f.walls[b];
						WARN(fmt::format("vres row {} ({:.3f}): wall i {} V {}| run {}; wall j {} V {}| run {}", tag, slack, wall_name(wi), vpts(wi.track, wi.piece, wi.piece + 1), rpts(wi.track, wi.piece, wi.piece + 1), wall_name(wj), vpts(wj.track, wj.piece, wj.piece + 1), rpts(wj.track, wj.piece, wj.piece + 1)));
						WARN(fmt::format("vres row {} margins: {}", tag, margins.contains(tag) ? margins[tag] : std::string("(none)")));
						const size_t n = f.walls.size();
						for (const auto &[from, to, name] : {std::tuple{a, b, "i to j"}, std::tuple{b, a, "j to i"}}) {
							std::string walk;
							for (size_t k = (from + 1) % n; k != to; k = (k + 1) % n) walk += fmt::format(" [{} len {}]", wall_name(f.walls[k]), wall_len(f.walls[k]));
							WARN(fmt::format("vres row {} walk {}:{}", tag, name, walk.empty() ? " (adjacent)" : walk));
						}
						const auto &hi = hybrid[wi.track], &hj = hybrid[wj.track];
						const bool vshare = wi.piece + 1 < hi.size() && wj.piece + 1 < hj.size() && (hi[wi.piece] == hj[wj.piece] || hi[wi.piece] == hj[wj.piece + 1] || hi[wi.piece + 1] == hj[wj.piece] || hi[wi.piece + 1] == hj[wj.piece + 1]);
						bool node_meet = false;
						if (wi.track < map.expanded.lines.size() && wj.track < map.expanded.lines.size() && wi.track != wj.track) {
							const auto [fi, ti] = ends_of(wi.track);
							const auto [fj, tj] = ends_of(wj.track);
							for (const PlanarEnd *x : {&fi, &ti}) {
								for (const PlanarEnd *y : {&fj, &tj}) node_meet = node_meet || (!x->port && !y->port && same_end(*x, *y));
							}
						}
						WARN(fmt::format("vres row {} shares V endpoint {}, tracks meet at a node {}, pieces {} {}", tag, vshare ? "yes" : "no", node_meet ? "yes" : "no", wi.track < shaped.tracks.size() ? shaped.tracks[wi.track].pieces.size() : 0, wj.track < shaped.tracks.size() ? shaped.tracks[wj.track].pieces.size() : 0));
					}
				} else if (tag.starts_with("node ")) {
					uint32_t n = UINT32_MAX;
					std::from_chars(tag.data() + 5, tag.data() + tag.size(), n);
					std::string text;
					if (n < map.expanded.nodes.size()) {
						for (const uint32_t line : map.expanded.nodes[n].in) {
							const size_t np = hybrid[line].size();
							text += fmt::format(" in {} V {}", map.tracks[line].name, np >= 2 ? vpts(line, np - 2, np - 1) : std::string("(none)"));
						}
						for (const uint32_t line : map.expanded.nodes[n].out) text += fmt::format(" out {} V {}", map.tracks[line].name, hybrid[line].size() >= 2 ? vpts(line, 0, 1) : std::string("(none)"));
					} else {
						text = " (tag not decoded)";
					}
					WARN(fmt::format("vres row {} ({:.3f}):{}", tag, slack, text));
				} else if (tag.starts_with("cross ")) {
					uint32_t n = UINT32_MAX;
					std::from_chars(tag.data() + 6, tag.data() + tag.size(), n);
					std::string text = " (tag not decoded)";
					if (n < map.crossings.size()) {
						text.clear();
						for (const uint32_t t : {map.crossings[n].a, map.crossings[n].b}) text += fmt::format(" {} V all points {}", map.tracks[t].name, vpts(t, 0, hybrid[t].empty() ? 0 : hybrid[t].size() - 1));
					}
					WARN(fmt::format("vres row {} ({:.3f}):{}", tag, slack, text));
				} else if (tag.starts_with("end ") || tag.starts_with("deck ")) {
					uint32_t n = UINT32_MAX;
					std::from_chars(tag.data() + tag.find(' ') + 1, tag.data() + tag.size(), n);
					std::string text = " (tag not decoded)";
					if (n < shaped.structure.structures.size()) {
						const PlanarStructure &st = shaped.structure.structures[n];
						text = fmt::format(" separator {} V all points {}", map.tracks[st.separator].name, vpts(st.separator, 0, hybrid[st.separator].empty() ? 0 : hybrid[st.separator].size() - 1));
						for (const uint32_t c : st.crossings) {
							for (const uint32_t t : {map.crossings[c].a, map.crossings[c].b}) text += fmt::format(" | {} V all points {}", map.tracks[t].name, vpts(t, 0, hybrid[t].empty() ? 0 : hybrid[t].size() - 1));
						}
					}
					WARN(fmt::format("vres row {} ({:.3f}):{}", tag, slack, text));
				} else {
					WARN(fmt::format("vres row {} ({:.3f}): (not decoded)", tag, slack));
				}
			}
			WarnSepGroups(shaped, r, "vres sides V+full");
		}
	}
	/* A probe (PLANAR_PROBE_INWARD): the same evaluation with inward candidates only, against the all-candidates run above's numbers, one run each. */
	if (std::getenv("PLANAR_PROBE_INWARD") != nullptr) {
		const std::vector<PlanarFaceWalls> face_walls = PlanarShapeFaceWalls(shaped);
		for (const bool inward : {false, true}) {
			const std::string label = inward ? "sepV allfaces W1 inward" : "sepV allfaces W1 all candidates";
			const PlanarDrawResult r = CheckPlanarDrawable(shaped, 60.0, {.clearance = true, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .inward_sides = inward, .elastic = true, .elastic_families = {"sep", "clear", "deck", "end", "node", "cross"}, .pins = &p8, .sides_from = &full.points, .fix_points = &vref, .fix_window = 1});
			WARN(fmt::format("{}: status {}, total slack {:.3f}, pairs {}, short_sides {}, inward_short {}", label, r.status, r.slack, r.pairs, r.short_sides, r.inward_short));
			if (r.status != "Optimal") {
				for (const std::string &tag : r.conflict) WARN(fmt::format("{} conflict {}", label, tag));
				continue;
			}
			double sep_slack = 0;
			size_t sep_rows = 0;
			for (const auto &[tag, slack] : r.stretched) {
				if (!tag.starts_with("sep ")) continue;
				sep_rows++;
				sep_slack += slack;
			}
			WARN(fmt::format("{}: sep slack {:.3f}, rows {}", label, sep_slack, sep_rows));
			for (const std::string &tag : r.inward_short_tags) {
				std::istringstream in(tag.substr(4));
				uint32_t face = 0;
				size_t a = 0, b = 0;
				in >> face >> a >> b;
				std::string text;
				for (const PlanarFaceWalls &f : face_walls) {
					if (f.face != face || a >= f.walls.size() || b >= f.walls.size()) continue;
					for (const size_t k : {a, b}) {
						const PlanarCellWall &w = f.walls[k];
						text += fmt::format(" [{}#{} h{}]", map.tracks[w.track].name, w.piece, w.heading);
					}
				}
				WARN(fmt::format("{} short {} |{}", label, tag, text));
			}
		}
	}
	/* The same, each pair's direction from the monotone face walk where the rule fixes it. */
	{
		const std::string label = "sepV walk W1";
		const PlanarDrawResult r = CheckPlanarDrawable(shaped, 60.0, {.clearance = true, .clear_rows = true, .pairwise = true, .elastic = true, .elastic_families = {"sep", "clear", "deck", "end", "node", "cross"}, .pins = &p8, .sides_from = &full.points, .sides_by_walk = true, .fix_points = &vref, .fix_window = 1});
		WARN(fmt::format("{}: status {}, total slack {:.3f}, pairs {}, walk_fixed {}, walk_short {}, walk_differs {}", label, r.status, r.slack, r.pairs, r.walk_fixed, r.walk_short, r.walk_differs));
		if (r.status == "Optimal") {
			std::map<std::string, std::pair<size_t, double>> families;
			for (const auto &[tag, slack] : r.stretched) {
				auto &f = families[tag.substr(0, tag.find(' '))];
				f.first++;
				f.second += slack;
			}
			for (const auto &[family, f] : families) WARN(fmt::format("{} family {}: rows {}, slack {:.3f}", label, family, f.first, f.second));
			WarnSepWalls(shaped, r, label, 20);
		} else {
			for (const std::string &tag : r.conflict) WARN(fmt::format("{} conflict {}", label, tag));
		}
	}
	/* The same, the walk rule fixes a pair only when its guaranteed projection keeps the spacing (D417). */
	{
		const std::string label = "sepV walk strict W1";
		const PlanarDrawResult r = CheckPlanarDrawable(shaped, 60.0, {.clearance = true, .clear_rows = true, .pairwise = true, .elastic = true, .elastic_families = {"sep", "clear", "deck", "end", "node", "cross"}, .pins = &p8, .sides_from = &full.points, .sides_by_walk = true, .walk_strict = true, .fix_points = &vref, .fix_window = 1});
		WARN(fmt::format("{}: status {}, total slack {:.3f}, pairs {}, walk_fixed {}, walk_short {}, walk_differs {}", label, r.status, r.slack, r.pairs, r.walk_fixed, r.walk_short, r.walk_differs));
		if (r.status == "Optimal") {
			std::map<std::string, std::pair<size_t, double>> families;
			for (const auto &[tag, slack] : r.stretched) {
				auto &f = families[tag.substr(0, tag.find(' '))];
				f.first++;
				f.second += slack;
			}
			for (const auto &[family, f] : families) WARN(fmt::format("{} family {}: rows {}, slack {:.3f}", label, family, f.first, f.second));
			WarnSepWalls(shaped, r, label, 20);
		} else {
			for (const std::string &tag : r.conflict) WARN(fmt::format("{} conflict {}", label, tag));
		}
	}
	PlanarShapeVariant geo = shaped;
	const std::vector<PlanarShapeDummy> rule_dummies = shaped.dummies;
	std::string why;
	if (!RefinePlanarShapeCells(geo, why, &full.points)) {
		WARN(fmt::format("geo refinement failed: {}", why));
		return;
	}
	size_t differ = 0;
	for (size_t i = 0; i < geo.dummies.size(); i++) {
		if (i >= rule_dummies.size()) {
			differ++;
			continue;
		}
		const PlanarShapeDummy &x = geo.dummies[i], &y = rule_dummies[i];
		if (x.face != y.face || x.track != y.track || x.piece != y.piece || x.heading != y.heading || x.land_track != y.land_track || x.land_piece != y.land_piece) differ++;
	}
	if (rule_dummies.size() != geo.dummies.size()) differ += rule_dummies.size() > geo.dummies.size() ? rule_dummies.size() - geo.dummies.size() : 0;
	WARN(fmt::format("geo refinement: dummies {} (rule {}), nonmonotone {}, landing fallbacks {}, differing from the rule's {}", geo.dummies.size(), rule_dummies.size(), geo.nonmonotone_dummies, geo.landing_fallbacks, differ));
	evaluate(geo, "geo");
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
	PlanarDrawPins p4 = pins;
	p4.window = 4;
	PlanarDrawPins p2 = pins;
	p2.window = 2;
	for (const auto &[name, p] : std::vector<std::pair<std::string, const PlanarDrawPins *>>{{"no pins", &none}, {"pins W8", &p8}, {"pins W4", &p4}, {"pins W2", &p2}}) {
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

TEST_CASE("planar draw: PSG 197 option A pairwise integer", "[.][planarkeepapart]")
{
	PlanarShapeVariant shaped;
	PlanarDrawPins pins;
	if (!LoadPinned197(shaped, pins, true)) return;
	if (!BuildOptionA(shaped)) return;
	for (const int window : {8, 4}) {
		for (const double seconds : {15.0, 60.0}) {
			PlanarDrawPins pW = pins;
			pW.window = window;
			const PlanarDrawResult r = CheckPlanarDrawable(shaped, seconds, {.clearance = true, .clear_rows = true, .pairwise = true, .sides_by_walk = true, .walk_strict = true, .pins = &pW});
			WARN(fmt::format("A pairwise integer strict W{} limit {:.0f} s: status {}, ms {:.0f}, pairs {}, walk_fixed {}, binaries {}", window, seconds, r.status, r.ms, r.pairs, r.walk_fixed, r.binaries));
			if (r.status == "Infeasible") {
				std::string tags;
				for (size_t i = 0; i < r.conflict.size() && i < 30; i++) tags += " " + r.conflict[i];
				WARN(fmt::format("A pairwise integer strict W{} limit {:.0f} s conflict:{}", window, seconds, tags));
			}
		}
	}

	/* D417 isolation: W8, 15 s, each run drops one ingredient of the infeasible run. */
	PlanarDrawPins p8 = pins;
	p8.window = 8;
	const auto report = [&](const char *name, const PlanarDrawResult &r) {
		WARN(fmt::format("D417 isolate {}: status {}, ms {:.0f}, pairs {}, walk_fixed {}, binaries {}", name, r.status, r.ms, r.pairs, r.walk_fixed, r.binaries));
		if (r.status == "Infeasible") {
			std::string tags;
			for (size_t i = 0; i < r.conflict.size() && i < 30; i++) tags += " " + r.conflict[i];
			WARN(fmt::format("D417 isolate {} conflict:{}", name, tags));
		}
	};
	report("1 no pins", CheckPlanarDrawable(shaped, 15.0, {.clearance = true, .clear_rows = true, .pairwise = true, .sides_by_walk = true, .walk_strict = true, .pins = nullptr}));
	report("2 pins, no walk rule", CheckPlanarDrawable(shaped, 15.0, {.clearance = true, .clear_rows = true, .pairwise = true, .sides_by_walk = false, .walk_strict = true, .pins = &p8}));
	report("3 pins, no clear rows", CheckPlanarDrawable(shaped, 15.0, {.clearance = true, .clear_rows = false, .pairwise = true, .sides_by_walk = true, .walk_strict = true, .pins = &p8}));

	/* Convex faces separated pairwise too, W8, 15 s, all binaries (a) and without pins (b). */
	const auto report_all = [&](const char *name, const PlanarDrawResult &r) {
		WARN(fmt::format("allfaces {}: status {}, ms {:.0f}, pairs {}, binaries {}", name, r.status, r.ms, r.pairs, r.binaries));
		if (r.status == "Infeasible") {
			std::string tags;
			for (size_t i = 0; i < r.conflict.size() && i < 30; i++) tags += " " + r.conflict[i];
			WARN(fmt::format("allfaces {} conflict:{}", name, tags));
		}
	};
	report_all("a pins", CheckPlanarDrawable(shaped, 15.0, {.clearance = true, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .pins = &p8}));
	report_all("b no pins", CheckPlanarDrawable(shaped, 15.0, {.clearance = true, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .pins = nullptr}));

	/* Detail of sep 155 2 5: replays the rule of the pairwise block on the face walk. */
	const PlanarTrackMap &map = shaped.structure.map;
	const PlanarShapeVariant &v = shaped;
	for (const PlanarFaceWalls &f : PlanarShapeFaceWalls(shaped)) {
		if (f.face != 155) continue;
		const std::vector<PlanarCellWall> &w = f.walls;
		const size_t n = w.size();
		const size_t i = 2, j = 5;
		const auto may_zero = [&](const PlanarCellWall &wall) {
			const PlanarShapeTrack &tr = v.tracks[wall.track];
			std::set<uint32_t> zero;
			for (size_t k = 0; k < tr.flex.size(); k++) {
				const uint32_t m = tr.flex[k];
				if (k == 0 || tr.flex[k - 1] + 2 != m) zero.insert(m - 1);
				zero.insert(m);
				zero.insert(m + 1);
			}
			for (size_t k = 0; k < tr.steps.size(); k++) {
				const uint32_t m = tr.steps[k];
				if (k == 0 || tr.steps[k - 1] + 2 != m) zero.insert(m - 1);
				zero.insert(m);
				zero.insert(m + 1);
			}
			for (const uint32_t m : tr.steps) zero.erase(m);
			const bool port_stretch = map.expanded.lines[wall.track].from.port != map.expanded.lines[wall.track].to.port && map.events[wall.track].empty();
			return port_stretch || zero.contains(wall.piece);
		};
		const auto name = [&](const PlanarCellWall &wall) { return fmt::format("{} piece {} heading {}", map.tracks[wall.track].name, wall.piece, wall.heading); };
		const std::array<int, 2> ni = PlanarModel::STEP[(w[i].heading + 6) % 8];
		const std::array<int, 2> nj = PlanarModel::STEP[(w[j].heading + 6) % 8];
		const int spacing0 = PlanarModel::RungSpacing(w[i].heading, w[j].heading);
		const int spacing1 = PlanarModel::RungSpacing(w[j].heading, w[i].heading);
		WARN(fmt::format("D417 sep 155 2 5: n {} walls, wall 2 [{}], wall 5 [{}], headings mod 4 {}", n, name(w[i]), name(w[j]), w[i].heading % 4 != w[j].heading % 4 ? "differ" : "equal"));
		WARN(fmt::format("D417 sep 155 2 5: axis0 normal ({},{}) spacing {}; axis1 normal ({},{}) spacing {}", ni[0], ni[1], spacing0, nj[0], nj[1], spacing1));
		const auto walk = [&](const char *label, size_t from, size_t to, const std::array<int, 2> &normal, int spacing) {
			bool monotone = true;
			int sum = 0;
			for (size_t k = (from + 1) % n;; k = (k + 1) % n) {
				const std::array<int, 2> step = PlanarModel::STEP[w[k].heading % 8];
				const int dot = step[0] * normal[0] + step[1] * normal[1];
				if (dot < 0) monotone = false;
				if (k == to) break;
				const bool zero = may_zero(w[k]);
				const int least = zero ? 0 : ((w[k].heading % 2) == 0 ? 2 : 1);
				sum += least * dot;
				WARN(fmt::format("D417 sep 155 2 5: {} between wall {} [{}] least units {} dot {}", label, k, name(w[k]), least, dot));
			}
			WARN(fmt::format("D417 sep 155 2 5: {} walk monotone {}, guaranteed {}, spacing {}, rule-fixed {}", label, monotone, sum, spacing, monotone && sum >= spacing));
		};
		walk("i->j on axis0", i, j, ni, spacing0);
		if (w[i].heading % 4 != w[j].heading % 4) {
			walk("j->i on axis1", j, i, nj, spacing1);
		} else {
			walk("j->i on axis0 (same-axis case)", j, i, nj, spacing0);
		}
	}
}

TEST_CASE("planar draw: PSG 197 export for offline solvers", "[.][planarkeepapart]")
{
	const std::string dir = "/tmp/claude-1000/-home-pete-OpenTTD/c9f5c621-e02c-4bb1-8894-876529c77fba/scratchpad/export/";
	std::filesystem::create_directories(dir);
	PlanarShapeVariant a;
	PlanarDrawPins a_pins;
	if (!LoadPinned197(a, a_pins, true)) return;
	if (!BuildOptionA(a)) return;
	PlanarShapeVariant d402;
	PlanarDrawPins d402_pins;
	if (!LoadPinned197(d402, d402_pins, false)) return;
	for (const auto &[name, variant, pins] : {std::tuple{"A", &a, &a_pins}, std::tuple{"D402", &d402, &d402_pins}}) {
		PlanarDrawPins p8 = *pins;
		p8.window = 8;
		for (const bool pinned : {true, false}) {
			const std::string file = fmt::format("{}{}-{}.json", dir, name, pinned ? "pins8" : "nopins");
			const PlanarDrawResult r = CheckPlanarDrawable(*variant, 1.0, {.clearance = true, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .pins = pinned ? &p8 : nullptr, .export_path = file});
			WARN(fmt::format("{}: status {}, variables {}, rows {}, binaries {}, pairs {}, size {} bytes", file, r.status, r.columns, r.rows, r.binaries, r.pairs, std::filesystem::exists(file) ? std::filesystem::file_size(file) : 0));
		}
	}
}

/** "track piece heading", marked "insert" for a flex insert and "near" when a flex middle is at piece +- 1. */
static std::string WallFlexText(const PlanarShapeVariant &shaped, const PlanarCellWall &w)
{
	if (w.track >= shaped.tracks.size()) return fmt::format("[track {} piece {} heading {}]", w.track, w.piece, w.heading);
	const std::vector<uint32_t> &flex = shaped.tracks[w.track].flex;
	const bool insert = std::ranges::find(flex, w.piece) != flex.end();
	const bool near = std::ranges::find(flex, w.piece + 1) != flex.end() || (w.piece > 0 && std::ranges::find(flex, w.piece - 1) != flex.end());
	return fmt::format("[{} piece {} heading {} insert {} near {}]", shaped.structure.map.tracks[w.track].name, w.piece, w.heading, insert ? "yes" : "no", near ? "yes" : "no");
}

/** WARN a `sep <face> <a> <b>` tag's two walls with their flex marks, and with `whole_face` the face's full wall list. */
static void WarnSepTagWalls(const PlanarShapeVariant &shaped, const std::string &tag, const std::string &row, bool whole_face)
{
	if (!row.starts_with("sep ")) return;
	std::istringstream in(row.substr(4));
	uint32_t face = 0;
	size_t a = 0, b = 0;
	in >> face >> a >> b;
	for (const PlanarFaceWalls &f : PlanarShapeFaceWalls(shaped)) {
		if (f.face != face || a >= f.walls.size() || b >= f.walls.size()) continue;
		WARN(fmt::format("{} {}: wall {} {} wall {} {}", tag, row, a, WallFlexText(shaped, f.walls[a]), b, WallFlexText(shaped, f.walls[b])));
		if (!whole_face) continue;
		std::string text;
		for (size_t k = 0; k < f.walls.size(); k++) text += fmt::format(" {}:{}", k, WallFlexText(shaped, f.walls[k]));
		WARN(fmt::format("{} {}: face {} walls in walk order:{}", tag, row, face, text));
	}
}

TEST_CASE("planar draw: PSG 197 option (1), directions from earlier drawings", "[.][planarkeepapart]")
{
	const auto start_time = std::chrono::steady_clock::now();
	const auto over_cap = [&]() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count() > 900.0; };
	const std::vector<std::string> all = {"point", "axis", "diagonal", "param", "cross", "other"};

	/* V's reference: the bounding box of every pin and the sum of every line's lengths. */
	{
		std::ifstream in(std::string(JUNCTION_TOPOLOGY_DIR) + "/../corpus/out/psg197/pins.txt");
		std::string text;
		int x0 = INT32_MAX, x1 = INT32_MIN, y0 = INT32_MAX, y1 = INT32_MIN;
		while (std::getline(in, text)) {
			if (text.empty() || text[0] == '#') continue;
			std::istringstream ls(text);
			std::vector<std::string> words;
			std::string w;
			while (ls >> w) words.push_back(w);
			if (words.size() < 2) continue;
			int x = 0, y = 0;
			std::istringstream(words[words.size() - 2]) >> x;
			std::istringstream(words[words.size() - 1]) >> y;
			x0 = std::min(x0, x); x1 = std::max(x1, x);
			y0 = std::min(y0, y); y1 = std::max(y1, y);
		}
		std::ifstream lin(std::string(JUNCTION_TOPOLOGY_DIR) + "/../corpus/out/psg197/lengths.txt");
		long total = 0;
		while (std::getline(lin, text)) {
			if (!text.starts_with("lengths ")) continue;
			std::istringstream ls(text.substr(text.find(':') + 1));
			int x;
			while (ls >> x) total += x;
		}
		WARN(fmt::format("V reference: pins bounding box {} x {} half tiles, total length {}", x1 - x0, y1 - y0, total));
	}

	const auto describe = [](const PlanarDrawResult &r) {
		long total = 0;
		int x0 = INT32_MAX, x1 = INT32_MIN, y0 = INT32_MAX, y1 = INT32_MIN;
		for (const auto &track : r.points) {
			for (size_t i = 0; i < track.size(); i++) {
				x0 = std::min(x0, track[i][0]); x1 = std::max(x1, track[i][0]);
				y0 = std::min(y0, track[i][1]); y1 = std::max(y1, track[i][1]);
				if (i > 0) total += std::max(std::abs(track[i][0] - track[i - 1][0]), std::abs(track[i][1] - track[i - 1][1]));
			}
		}
		if (x0 > x1) return fmt::format("status {}, ms {:.0f}, no points", r.status, r.ms);
		return fmt::format("status {}, ms {:.0f}, short_sides {}, box {} x {}, total length {}", r.status, r.ms, r.short_sides, x1 - x0, y1 - y0, total);
	};

	PlanarShapeVariant a;
	PlanarDrawPins a_pins;
	if (!LoadPinned197(a, a_pins, true)) return;
	if (!BuildOptionA(a)) return;
	PlanarShapeVariant d402;
	PlanarDrawPins d402_pins;
	if (!LoadPinned197(d402, d402_pins, false)) return;
	PlanarShapeVariant flex_c;
	PlanarDrawPins flex_c_pins;
	if (!LoadPinned197(flex_c, flex_c_pins, true)) return;

	for (const auto &[name, variant, pins] : {std::tuple{"A", &a, &a_pins}, std::tuple{"D402", &d402, &d402_pins}, std::tuple{"C", &flex_c, &flex_c_pins}}) {
		PlanarDrawPins p8 = *pins;
		p8.window = 8;
		for (const bool pinned : {true, false}) {
			if (variant == &flex_c && pinned) continue;
			if (over_cap()) {
				WARN("total cap of 15 minutes passed; stopped");
				return;
			}
			const PlanarDrawPins *p = pinned ? &p8 : nullptr;
			const std::string label = fmt::format("{} {}", name, pinned ? "W8" : "no pins");
			/* PLANAR_PROBE_CASE (a measurement aid) runs only the case with that label, e.g. "C no pins". */
			if (const char *only = std::getenv("PLANAR_PROBE_CASE"); only != nullptr && label != only) continue;
			const PlanarDrawResult seed = CheckPlanarDrawable(*variant, 30.0, {.elastic = true, .elastic_tidy = 0.001, .pins = p});
			WARN(fmt::format("{} seed: {}, total slack {:.3f}", label, describe(seed), seed.slack));
			if (!seed.feasible) continue;
			/* PLANAR_PROBE_PAD (a measurement aid): rounds raise every separation's spacing by this; finals do not. */
			const char *pad_text = std::getenv("PLANAR_PROBE_PAD");
			int round_pad = 0;
			if (pad_text != nullptr) std::from_chars(pad_text, pad_text + std::strlen(pad_text), round_pad);
			if (round_pad != 0) WARN(fmt::format("{} rounds padded by {}", label, round_pad));
			/* PLANAR_PROBE_EXTREMAL (a measurement aid): rounds and finals write one separation row per pair (sep_extremal). */
			const bool extremal = std::getenv("PLANAR_PROBE_EXTREMAL") != nullptr;
			/* PLANAR_PROBE_FLEX_PRICE (a measurement aid, D422): the rounds price insert length; PLANAR_PROBE_COMMIT_ABOVE: the ratchet commits inserts at or above this (half tiles, default 0.25). */
			double flex_price = 0, commit_above = 0.25;
			if (const char *fp = std::getenv("PLANAR_PROBE_FLEX_PRICE"); fp != nullptr) std::from_chars(fp, fp + std::strlen(fp), flex_price);
			if (const char *ca = std::getenv("PLANAR_PROBE_COMMIT_ABOVE"); ca != nullptr) std::from_chars(ca, ca + std::strlen(ca), commit_above);
			/* PLANAR_PROBE_IRL1=C (a measurement aid, D422): from round 2 each insert's price is C / (its previous length + 0.5) (iteratively reweighted L1); PLANAR_PROBE_BOX: price on width and height in the rounds and the ratchet. */
			double irl1 = 0, box_price = 0;
			if (const char *ir = std::getenv("PLANAR_PROBE_IRL1"); ir != nullptr) std::from_chars(ir, ir + std::strlen(ir), irl1);
			if (const char *bp = std::getenv("PLANAR_PROBE_BOX"); bp != nullptr) std::from_chars(bp, bp + std::strlen(bp), box_price);
			std::vector<std::vector<double>> irl1_weights;
			if (irl1 != 0 || box_price != 0) WARN(fmt::format("{} IRL1 {}, box price {}", label, irl1, box_price));
			if (flex_price != 0 || commit_above != 0.25) WARN(fmt::format("{} flex price {}, commit above {}", label, flex_price, commit_above));
			if (extremal) WARN(fmt::format("{} one separation row per pair", label));
			const PlanarDrawResult *previous = &seed;
			std::vector<PlanarDrawResult> rounds;
			rounds.reserve(8);
			const PlanarDrawResult *last_optimal = nullptr;
			for (int k = 1; k <= 8; k++) {
				if (over_cap()) {
					WARN("total cap of 15 minutes passed; stopped");
					return;
				}
				rounds.push_back(CheckPlanarDrawable(*variant, 30.0, {.clearance = true, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .sep_extremal = extremal, .flex_weights = irl1_weights.empty() ? nullptr : &irl1_weights, .box_price = box_price, .flex_price = flex_price, .elastic = true, .elastic_tidy = 0.001, .elastic_families = {"sep"}, .pins = p, .sides_from = &previous->points, .sides_pad = round_pad}));
				const PlanarDrawResult &r = rounds.back();
				WARN(fmt::format("{} round {}: {}, total slack {:.3f}", label, k, describe(r), r.slack));
				if (k == 1 && variant == &flex_c) WARN(fmt::format("{} pairs {}", label, r.pairs));
				if (r.status != "Optimal") {
					std::string conflicts;
					for (const std::string &c : r.conflict) conflicts += c + "; ";
					WARN(fmt::format("{} round {} conflict: {}", label, k, conflicts));
					break;
				}
				const bool stalled = k > 1 && previous->slack - r.slack <= 0.5;
				/* PLANAR_PROBE_ZERO_TWICE (a measurement aid): stop only when a round read from a slack-0 round's rounded output is at slack 0 too, so the final's sides (with PLANAR_PROBE_FINAL_SIDES=input) are both LP-proved and read from a whole-number drawing; at most 8 rounds. */
				const bool zero_twice = std::getenv("PLANAR_PROBE_ZERO_TWICE") != nullptr;
				const bool previous_zero = k > 1 && previous->slack < 1e-6;
				last_optimal = &r;
				previous = &r;
				if (irl1 != 0) {
					irl1_weights.assign(variant->tracks.size(), {});
					for (size_t t = 0; t < variant->tracks.size() && t < r.lengths.size(); t++) {
						irl1_weights[t].assign(r.lengths[t].size(), 0.0);
						for (size_t i = 0; i < r.lengths[t].size(); i++) irl1_weights[t][i] = irl1 / (r.lengths[t][i] + 0.5);
					}
				}
				if (zero_twice) {
					if (r.slack < 1e-6 && previous_zero) break;
					continue;
				}
				if (r.slack < 1e-6 || stalled) break;
			}
			if (last_optimal == nullptr) {
				WARN(fmt::format("{} no Optimal round; no final", label));
				continue;
			}
			const bool plateau_probe = (variant == &d402 || variant == &flex_c) && !pinned;
			if (plateau_probe) {
				for (const auto &[row, slack] : last_optimal->stretched) {
					WARN(fmt::format("{} plateau stretched {}: {:.3f}", label, row, slack));
					WarnSepTagWalls(*variant, label + " plateau", row, false);
				}
			}
			bool conflict_shown = false;
			/* PLANAR_PROBE_FLEXZERO (a measurement aid): histogram of the last round's flex insert lengths, then hold inserts below the threshold at 0, one more sep-only round, and an integer final from it. */
			if (const char *zero_text = std::getenv("PLANAR_PROBE_FLEXZERO"); zero_text != nullptr) {
				double below = 0;
				std::from_chars(zero_text, zero_text + std::strlen(zero_text), below);
				std::array<int, 6> bins{};
				int fractional = 0;
				for (size_t t = 0; t < variant->tracks.size() && t < last_optimal->lengths.size(); t++) {
					for (const uint32_t m : variant->tracks[t].flex) {
						if (m >= last_optimal->lengths[t].size()) continue;
						const double l = last_optimal->lengths[t][m];
						bins[l < 1e-6 ? 0 : l < 0.5 ? 1 : l < 1 ? 2 : l < 2 ? 3 : l < 4 ? 4 : 5]++;
						if (std::abs(l - std::round(l)) > 1e-6) fractional++;
					}
				}
				WARN(fmt::format("{} flex insert lengths: 0: {}, (0,0.5): {}, [0.5,1): {}, [1,2): {}, [2,4): {}, >=4: {}; fractional {}", label, bins[0], bins[1], bins[2], bins[3], bins[4], bins[5], fractional));
				const PlanarDrawResult z = CheckPlanarDrawable(*variant, 30.0, {.clearance = true, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .elastic = true, .elastic_tidy = 0.001, .elastic_families = {"sep"}, .pins = p, .sides_from = &last_optimal->points, .flex_zero_from = &last_optimal->lengths, .flex_zero_below = below});
				WARN(fmt::format("{} flex-zero round (below {:.2f}): {}, total slack {:.3f}", label, below, describe(z), z.slack));
				if (z.feasible) {
					const PlanarDrawResult fz = CheckPlanarDrawable(*variant, 15.0, {.clearance = true, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .pins = p, .sides_from = &z.points, .flex_zero_from = &last_optimal->lengths, .flex_zero_below = below});
					WARN(fmt::format("{} flex-zero final (integer, 15 s): {}", label, describe(fz)));
					if (fz.status == "Infeasible") {
						std::string conflicts;
						for (size_t i = 0; i < std::min<size_t>(30, fz.conflict.size()); i++) conflicts += fz.conflict[i] + "; ";
						WARN(fmt::format("{} flex-zero final conflict: {}", label, conflicts));
					}
				}
				continue;
			}
			/* PLANAR_PROBE_CONTINUOUS (a measurement aid): comma-separated variable groups the integer finals leave continuous, e.g. "axis". */
			std::vector<std::string> final_continuous;
			if (const char *groups = std::getenv("PLANAR_PROBE_CONTINUOUS"); groups != nullptr) {
				std::istringstream gs(groups);
				std::string g;
				while (std::getline(gs, g, ',')) final_continuous.push_back(g);
				WARN(fmt::format("{} finals leave continuous: {}", label, groups));
			}
			/* PLANAR_PROBE_RATCHET (a measurement aid, D422): commit the inserts the last round used (curve rows on them, the rest at 0), re-pick sides with slack on sep and curve rows (at most 4 rounds), prune the committed inserts whose curve rows still stretch (once), then an integer final. */
			if (std::getenv("PLANAR_PROBE_RATCHET") != nullptr) {
				std::vector<std::vector<double>> committed = last_optimal->lengths;
				const auto count_committed = [&]() {
					size_t n = 0;
					for (size_t t = 0; t < variant->tracks.size() && t < committed.size(); t++) {
						for (const uint32_t m : variant->tracks[t].flex) n += m < committed[t].size() && committed[t][m] >= commit_above ? 1 : 0;
					}
					return n;
				};
				WARN(fmt::format("{} ratchet: committed {} inserts", label, count_committed()));
				const auto ratchet_rounds = [&](const PlanarDrawResult *from, const std::string &stage) {
					std::vector<PlanarDrawResult> rr;
					rr.reserve(4);
					const PlanarDrawResult *prev = from;
					for (int k = 1; k <= 4; k++) {
						rr.push_back(CheckPlanarDrawable(*variant, 30.0, {.clearance = true, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .sep_extremal = extremal, .curve_flex_from = &committed, .curve_flex_above = commit_above, .box_price = box_price, .elastic = true, .elastic_tidy = 0.001, .elastic_families = {"sep", "curve"}, .pins = p, .sides_from = &prev->points}));
						const PlanarDrawResult &r = rr.back();
						double curve_slack = 0;
						for (const auto &[row, slack] : r.stretched) curve_slack += row.starts_with("curve") ? slack : 0;
						WARN(fmt::format("{} ratchet {} round {}: {}, total slack {:.3f}, curve slack {:.3f}", label, stage, k, describe(r), r.slack, curve_slack));
						if (r.status != "Optimal") break;
						const bool done = r.slack < 1e-6 || (k > 1 && prev->slack - r.slack <= 0.5);
						prev = &rr.back();
						if (done) break;
					}
					return rr;
				};
				std::vector<PlanarDrawResult> commit = ratchet_rounds(last_optimal, "commit");
				if (commit.empty() || commit.back().status != "Optimal") continue;
				size_t pruned = 0;
				for (const auto &[row, slack] : commit.back().stretched) {
					if (!row.starts_with("curve")) continue;
					WARN(fmt::format("{} ratchet stretched {}: {:.3f}", label, row, slack));
					if (!row.starts_with("curve flex ")) continue;
					std::istringstream rs(row.substr(11));
					std::string name;
					uint32_t piece = 0;
					rs >> name >> piece;
					for (size_t t = 0; t < variant->tracks.size(); t++) {
						if (variant->structure.map.tracks[t].name != name || piece >= committed[t].size()) continue;
						committed[t][piece] = 0;
						pruned++;
					}
				}
				std::vector<PlanarDrawResult> prune;
				if (pruned > 0) {
					WARN(fmt::format("{} ratchet: pruned {}, committed now {}", label, pruned, count_committed()));
					prune = ratchet_rounds(&commit.back(), "prune");
					if (prune.empty() || prune.back().status != "Optimal") continue;
				}
				const std::vector<PlanarDrawResult> &final_rounds = pruned > 0 ? prune : commit;
				const PlanarDrawResult *sides = final_rounds.size() > 1 ? &final_rounds[final_rounds.size() - 2] : &commit.back();
				if (pruned > 0 && final_rounds.size() == 1) sides = &commit.back();
				const PlanarDrawResult f = CheckPlanarDrawable(*variant, 60.0, {.clearance = true, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .sep_extremal = extremal, .curve_flex_from = &committed, .curve_flex_above = commit_above, .pins = p, .sides_from = &sides->points});
				WARN(fmt::format("{} ratchet final (integer, 60 s): {}", label, describe(f)));
				continue;
			}
			/* PLANAR_PROBE_FINAL_SIDES=input (a measurement aid): the finals read their sides from the last Optimal round's own input drawing, i.e. the directions that round proved LP-feasible, not re-read from its rounded output. */
			const std::vector<std::vector<std::array<int, 2>>> *final_sides = &last_optimal->points;
			if (const char *fs = std::getenv("PLANAR_PROBE_FINAL_SIDES"); fs != nullptr && std::string_view(fs) == "input") {
				const size_t at = static_cast<size_t>(last_optimal - rounds.data());
				final_sides = at > 0 ? &rounds[at - 1].points : &seed.points;
				WARN(fmt::format("{} finals take the sides of round {} (its input drawing)", label, at + 1));
			}
			/* PLANAR_PROBE_FINAL_FLEXZERO (a measurement aid): the finals hold every flex insert the last Optimal round left below this at 0 (that round's drawing keeps them 0, so the final stays LP-feasible). */
			double final_zero_below = 0;
			if (const char *fz = std::getenv("PLANAR_PROBE_FINAL_FLEXZERO"); fz != nullptr) {
				std::from_chars(fz, fz + std::strlen(fz), final_zero_below);
				WARN(fmt::format("{} finals hold inserts below {} at 0", label, final_zero_below));
			}
			/* PLANAR_PROBE_START=lengths|points (a measurement aid): the finals get a MIP start from the last Optimal round's drawing: its whole lengths (partial) or its rounded points. */
			const std::vector<std::vector<double>> *start_lengths = nullptr;
			const std::vector<std::vector<std::array<int, 2>>> *start_points = nullptr;
			if (const char *st = std::getenv("PLANAR_PROBE_START"); st != nullptr) {
				if (std::string_view(st) == "lengths") start_lengths = &last_optimal->lengths;
				if (std::string_view(st) == "points") start_points = &last_optimal->points;
				WARN(fmt::format("{} finals start from the last round's {}", label, st));
			}
			for (const double budget : {15.0, 60.0}) {
				const PlanarDrawResult f = CheckPlanarDrawable(*variant, budget, {.clearance = true, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .sep_extremal = extremal, .pins = p, .sides_from = final_sides, .flex_zero_from = final_zero_below > 0 ? &last_optimal->lengths : nullptr, .flex_zero_below = final_zero_below, .continuous = final_continuous, .start_lengths = start_lengths, .start_points = start_points});
				WARN(fmt::format("{} final (integer, {:.0f} s): {}, start values {}", label, budget, describe(f), f.start_values));
				if (f.status == "Infeasible" && plateau_probe && !conflict_shown) {
					conflict_shown = true;
					for (const std::string &c : f.conflict) WarnSepTagWalls(*variant, label + " final conflict", c, true);
				}
				if (f.status == "Infeasible") {
					std::string conflicts;
					for (size_t i = 0; i < std::min<size_t>(30, f.conflict.size()); i++) conflicts += f.conflict[i] + "; ";
					WARN(fmt::format("{} final conflict: {}", label, conflicts));
				}
			}
			for (const double below : {2.0, 4.0}) {
				for (const double budget : {15.0}) {
					const PlanarDrawResult f = CheckPlanarDrawable(*variant, budget, {.clearance = true, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .pins = p, .sides_from = &last_optimal->points, .sides_free_below = below});
					WARN(fmt::format("{} final free below {:.0f} (integer, {:.0f} s): {}, sides_free {}, binaries {}", label, below, budget, describe(f), f.sides_free, f.binaries));
					if (f.status == "Infeasible") {
						std::string conflicts;
						for (size_t i = 0; i < std::min<size_t>(30, f.conflict.size()); i++) conflicts += f.conflict[i] + "; ";
						WARN(fmt::format("{} free below {:.0f} conflict: {}", label, below, conflicts));
					}
				}
			}
			if (plateau_probe) {
				const size_t at = static_cast<size_t>(last_optimal - rounds.data());
				const PlanarDrawResult &before = at > 0 ? rounds[at - 1] : seed;
				const PlanarDrawResult again = CheckPlanarDrawable(*variant, 30.0, {.clearance = true, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .elastic = true, .elastic_tidy = 0.001, .elastic_families = {"sep"}, .pins = p, .sides_from = &before.points});
				WARN(fmt::format("{} plateau sides from round {}: {}, total slack {:.3f}", label, at, again.status, again.slack));
				for (const auto &[row, slack] : again.stretched) {
					if (!row.starts_with("sep ")) continue;
					WARN(fmt::format("{} plateau-again stretched {}: {:.3f}", label, row, slack));
				}
			}
		}
	}
}

TEST_CASE("planar draw: the pipeline on PSG 197 (D422)", "[.][planarkeepapart]")
{
	const auto describe = [](const PlanarDrawResult &r) {
		long total = 0;
		int x0 = INT32_MAX, x1 = INT32_MIN, y0 = INT32_MAX, y1 = INT32_MIN;
		for (const auto &track : r.points) {
			for (size_t i = 0; i < track.size(); i++) {
				x0 = std::min(x0, track[i][0]); x1 = std::max(x1, track[i][0]);
				y0 = std::min(y0, track[i][1]); y1 = std::max(y1, track[i][1]);
				if (i > 0) total += std::max(std::abs(track[i][0] - track[i - 1][0]), std::abs(track[i][1] - track[i - 1][1]));
			}
		}
		if (x0 > x1) return fmt::format("status {}, ms {:.0f}, no points", r.status, r.ms);
		return fmt::format("status {}, ms {:.0f}, short_sides {}, box {} x {}, total length {}", r.status, r.ms, r.short_sides, x1 - x0, y1 - y0, total);
	};
	PlanarShapeVariant flex_c;
	PlanarDrawPins flex_c_pins;
	if (!LoadPinned197(flex_c, flex_c_pins, true)) return;
	const auto start = std::chrono::steady_clock::now();
	/* PLANAR_PROBE_FINAL_START (a measurement aid): the final starts from the last round's whole lengths.
	 * PLANAR_PROBE_FINAL_GRID: the final writes the grid rows (D382 probe).
	 * PLANAR_PROBE_SLACK_PRICE: the objective cost of each elastic slack per half tile from the commit on (default 1); PLANAR_PROBE_DISCOVERY_SLACK_PRICE: the same for the seed and discovery rounds (default 1). PLANAR_PROBE_FINAL_SOFT_SEP: the final's sep rows get slack (soft-sep diagnostic); PLANAR_PROBE_FINAL_SOFT_ALL: every row but piece, box, pin, fix and grid gets slack (feasibility repair); PLANAR_PROBE_FINAL_SECONDS: the final's time limit; PLANAR_PROBE_BOX_PRICE and PLANAR_PROBE_IRL1: the box price and the insert price; PLANAR_PROBE_PRUNES: at most this many prunes; PLANAR_PROBE_ADDITIONS: the one-time additions pass; PLANAR_PROBE_NO_NODE_GAPS: no node gaps (on by default, D424); PLANAR_PROBE_NO_CURVE_DISCOVERY: Scheme A curve-aware discovery. */
	PlanarDrawPipelineOptions o;
	o.final_start = std::getenv("PLANAR_PROBE_FINAL_START") != nullptr;
	o.final_tidy = std::getenv("PLANAR_PROBE_FINAL_FEASIBILITY") == nullptr;
	o.final_grid = std::getenv("PLANAR_PROBE_FINAL_GRID") != nullptr;
	o.final_soft_sep = std::getenv("PLANAR_PROBE_FINAL_SOFT_SEP") != nullptr;
	o.final_soft_all = std::getenv("PLANAR_PROBE_FINAL_SOFT_ALL") != nullptr;
	if (const char *s = std::getenv("PLANAR_PROBE_FINAL_SECONDS"); s != nullptr) o.final_seconds = std::atof(s);
	if (const char *s = std::getenv("PLANAR_PROBE_BOX_PRICE"); s != nullptr) o.box_price = std::atof(s);
	o.node_gaps = std::getenv("PLANAR_PROBE_NO_NODE_GAPS") == nullptr;
	if (const char *s = std::getenv("PLANAR_PROBE_IRL1"); s != nullptr) o.irl1 = std::atof(s);
	if (const char *s = std::getenv("PLANAR_PROBE_PRUNES"); s != nullptr) std::from_chars(s, s + std::strlen(s), o.prunes);
	if (const char *s = std::getenv("PLANAR_PROBE_SLACK_PRICE"); s != nullptr) o.slack_price = std::atof(s);
	if (const char *s = std::getenv("PLANAR_PROBE_DISCOVERY_SLACK_PRICE"); s != nullptr) o.discovery_slack_price = std::atof(s);
	o.additions = std::getenv("PLANAR_PROBE_ADDITIONS") != nullptr;
	o.curve_discovery = std::getenv("PLANAR_PROBE_NO_CURVE_DISCOVERY") == nullptr; // Scheme A (a probe).
	o.sibling_lanes = std::getenv("PLANAR_PROBE_SIBLINGS") != nullptr; // Sibling lanes as offset curves (a probe).
	o.facing_sides = std::getenv("PLANAR_PROBE_FACING") != nullptr; // D426 facing pairs (a probe).
	const PlanarDrawPipelineResult r = DrawPlanarPipeline(flex_c, o);
	if (o.facing_sides) WARN(fmt::format("197 facing: fixed {} choice {}", r.last_round.facing_fixed, r.last_round.facing_choice));
	if (o.sibling_lanes) {
		WARN(fmt::format("197 sibling lanes: pairs {} sep skipped {}", r.last_round.lane_pairs, r.last_round.lane_sep_skipped));
		for (const std::string &line : r.last_round.lane_skipped) WARN(fmt::format("197 sibling lanes skipped {}", line));
	}
	for (const std::string &line : r.log) WARN(line);
	WARN(fmt::format("committed {} inserts, pruned {}, added {}, stopped '{}'", r.committed_count, r.pruned, r.added, r.stopped));
	WARN(fmt::format("final: {}", describe(r.final)));
	std::map<std::string, std::pair<int, double>> families;
	for (const auto &[tag, amount] : r.last_round.stretched) {
		auto &f = families[tag.substr(0, tag.find(' ', tag.find(' ') + 1))];
		f.first++;
		f.second += amount;
	}
	std::string text;
	for (const auto &[family, f] : families) text += fmt::format(" [{} rows {} slack {:.2f}]", family, f.first, f.second);
	WARN(fmt::format("last round slack {:.3f}:{}", r.last_round.slack, text));
	const std::vector<PlanarFaceWalls> face_walls = PlanarShapeFaceWalls(flex_c);
	const auto piece_text = [&](uint32_t t, uint32_t piece) {
		if (t >= r.last_round.points.size() || piece + 1 >= r.last_round.points[t].size()) return std::string("?");
		const auto &a = r.last_round.points[t][piece];
		const auto &b = r.last_round.points[t][piece + 1];
		return fmt::format("{}#{} h{} ({},{})-({},{})", flex_c.structure.map.tracks[t].name, piece, ((flex_c.tracks[t].pieces[piece] % 8) + 8) % 8, a[0], a[1], b[0], b[1]);
	};
	for (const auto &[tag, amount] : r.last_round.stretched) {
		std::istringstream words(tag);
		std::string family;
		uint32_t face = 0, i = 0, j = 0;
		words >> family;
		if (family == "sep" && (words >> face >> i >> j)) {
			for (const PlanarFaceWalls &f : face_walls) {
				if (f.face != face || i >= f.walls.size() || j >= f.walls.size()) continue;
				const PlanarCellWall &a = f.walls[i];
				const PlanarCellWall &b = f.walls[j];
				WARN(fmt::format("decode {} {:.2f}: {} | {}", tag, amount, a.track < flex_c.tracks.size() ? piece_text(a.track, a.piece) : std::string("box"), b.track < flex_c.tracks.size() ? piece_text(b.track, b.piece) : std::string("box")));
			}
		} else {
			WARN(fmt::format("decode {} {:.2f}", tag, amount));
		}
	}
	/* Grid check (D382 probe): per axis, the parity of the across coordinate at both ends of every axis piece of positive length. On the game grid each axis has one parity. */
	std::array<std::array<int, 2>, 2> grid_parity{};
	for (size_t t = 0; t < r.final.points.size() && t < flex_c.tracks.size(); t++) {
		for (size_t i = 0; i < flex_c.tracks[t].pieces.size() && i < r.final.lengths[t].size() && i + 1 < r.final.points[t].size(); i++) {
			const int h = ((flex_c.tracks[t].pieces[i] % 8) + 8) % 8;
			if (h % 2 != 0 || r.final.lengths[t][i] < 0.5) continue;
			const int across = h % 4 == 0 ? 0 : 1;
			for (const size_t at : {i, i + 1}) grid_parity[across][((r.final.points[t][at][across] % 2) + 2) % 2]++;
		}
	}
	WARN(fmt::format("grid check: N/S piece ends x even {} odd {}; E/W piece ends y even {} odd {}", grid_parity[0][0], grid_parity[0][1], grid_parity[1][0], grid_parity[1][1]));
	if (o.final_grid) WARN(fmt::format("grid pass: forced {}, tightened {}, conflicts {}, rows {}, columns {}", r.final.grid_forced, r.final.grid_tightened, r.final.grid_conflicts, r.final.rows, r.final.columns));
	if (o.final_soft_sep || o.final_soft_all) {
		std::string top;
		for (size_t k = 0; k < r.final.stretched.size() && k < 30; k++) top += fmt::format(" [{} {:.2f}]", r.final.stretched[k].first, r.final.stretched[k].second);
		WARN(fmt::format("soft: slack {:.2f}, stretched tags {}:{}", r.final.slack, r.final.stretched.size(), top));
	}
	WARN(fmt::format("pipeline wall time {:.1f} s", std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count()));
	{
		std::ofstream tr("/home/pete/OpenTTD/.claude/ng1/build-rel/ourtravel.txt");
		for (size_t t = 0; t < r.last_round.points.size() && t < flex_c.tracks.size(); t++) {
			const auto &pts = r.last_round.points[t];
			if (pts.size() < 2) continue;
			int sum = 0;
			for (size_t i = 0; i + 1 < pts.size(); i++) sum += std::max(std::abs(pts[i + 1][0] - pts[i][0]), std::abs(pts[i + 1][1] - pts[i][1]));
			tr << flex_c.structure.map.tracks[t].name << " " << sum << " " << pts.size() - 1 << "\n";
		}
	}
	CHECK(r.final.status == "Optimal");
}

TEST_CASE("planar draw: beside tracks and their parent chains (corridor recon)", "[.][planarcorridor]")
{
	for (const uint32_t lanes : {1U, 2U}) {
		const DrawCase c = ShapeFirst(PlanarTText(lanes), 1);
		const PlanarTrackMap &map = c.v.structure.map;
		for (uint32_t b = 0; b < map.tracks.size(); b++) {
			if (map.part_of[b] != UINT32_MAX) continue;
			std::vector<uint32_t> chain;
			const bool ok = ParentChain(map, c.v.structure.sites, b, chain);
			std::string text = fmt::format("T{} beside {} from {} {} to {} {}: pieces", lanes, map.tracks[b].name, map.expanded.lines[b].from.port ? "port" : "node", map.expanded.lines[b].from.index, map.expanded.lines[b].to.port ? "port" : "node", map.expanded.lines[b].to.index);
			for (const int h : c.v.tracks[b].pieces) text += fmt::format(" {}", h);
			text += fmt::format(" flex {}; chain{}", c.v.tracks[b].flex.size(), ok ? "" : " NONE");
			for (const uint32_t t : chain) {
				text += fmt::format(" {}[", map.tracks[t].name);
				for (const int h : c.v.tracks[t].pieces) text += fmt::format("{} ", h);
				text += "]";
			}
			WARN(text);
		}
	}
}

TEST_CASE("planar draw: T1 curves with every flex at 0 (diagnosis)", "[.][planarcorridor]")
{
	const DrawCase c = ShapeFirst(PlanarTText(1), 1);
	std::vector<std::vector<double>> zero(c.v.tracks.size());
	for (size_t t = 0; t < c.v.tracks.size(); t++) zero[t].assign(c.v.tracks[t].pieces.size(), 0.0);
	for (const bool curves : {false, true}) {
		const PlanarDrawResult r = CheckPlanarDrawable(c.v, 10.0, {.clearance = false, .curves = curves, .flex_zero_from = &zero, .flex_zero_below = 1.0});
		std::string conflicts;
		for (const std::string &tag : r.conflict) conflicts += tag + "; ";
		WARN(fmt::format("T1 flex 0 curves {}: status {}, conflict {}", curves, r.status, conflicts));
		const PlanarDrawResult e = CheckPlanarDrawable(c.v, 10.0, {.clearance = false, .curves = curves, .elastic = true, .flex_zero_from = &zero, .flex_zero_below = 1.0});
		for (const auto &[row, slack] : e.stretched) WARN(fmt::format("T1 flex 0 curves {} stretched {}: {:.2f}", curves, row, slack));
	}
	for (size_t t = 0; t < c.v.tracks.size(); t++) {
		std::string text = fmt::format("{} pieces", c.v.structure.map.tracks[t].name);
		for (const int h : c.v.tracks[t].pieces) text += fmt::format(" {}", ((h % 8) + 8) % 8);
		text += " flex";
		for (const uint32_t m : c.v.tracks[t].flex) text += fmt::format(" {}", m);
		WARN(text);
	}
}

TEST_CASE("planar draw: T3 exact sides against the pipeline's", "[.][planarcert]")
{
	const DrawCase c = ShapeFirst(PlanarTText(3), 1);
	PlanarDrawPipelineOptions po;
	po.final_seconds = 5;
	const PlanarDrawPipelineResult r = DrawPlanarPipeline(c.v, po);

	PlanarDrawOptions eo;
	eo.clearance = true;
	eo.clear_rows = true;
	eo.pairwise = true;
	eo.pairwise_all_faces = true;
	eo.only_faces = {75, 29, 66};
	eo.corridor = true;
	eo.tidy = false;
	eo.continuous = {"point", "axis", "diagonal", "param", "cross"};
	eo.coord_max = 256;
	const PlanarDrawResult exact = CheckPlanarDrawable(c.v, 15.0, eo);
	if (exact.status != "Optimal") {
		WARN(fmt::format("exact solve is {}, stopping", exact.status));
		return;
	}

	const std::vector<PlanarFaceWalls> face_walls = PlanarShapeFaceWalls(c.v);
	struct Pair {
		uint32_t face, i, j;
		const PlanarFaceWalls *f;
		std::string tag;
	};
	std::vector<Pair> pairs;
	for (const uint32_t face : eo.only_faces) {
		for (const PlanarFaceWalls &f : face_walls) {
			if (f.face != face) continue;
			const size_t n = f.walls.size();
			for (size_t i = 0; i < n; i++) {
				for (size_t j = i + 1; j < n; j++) {
					if (j == i + 1 || (i == 0 && j == n - 1)) continue;
					if (f.walls[i].track >= c.v.tracks.size() || f.walls[j].track >= c.v.tracks.size()) continue;
					pairs.push_back({face, static_cast<uint32_t>(i), static_cast<uint32_t>(j), &f, fmt::format("sep {} {} {}", face, i, j)});
				}
			}
		}
	}

	const auto margins = [&](const std::vector<std::vector<std::array<int, 2>>> &from) {
		PlanarDrawOptions o{.clearance = true, .clear_rows = true, .pairwise = true, .pairwise_all_faces = true, .sep_extremal = true, .node_gaps = true, .node_gap_from = &r.last_round.lengths, .corridor = true, .curve_flex_from = &r.committed, .curve_flex_above = po.commit_above, .box_price = po.box_price, .elastic = true, .elastic_tidy = 0.001, .slack_price = po.slack_price, .elastic_families = {"sep", "curve"}, .sides_from = &from};
		for (const Pair &p : pairs) o.margin_tags.push_back(p.tag);
		const PlanarDrawResult m = CheckPlanarDrawable(c.v, 15.0, o);
		if (m.status != "Optimal") WARN(fmt::format("margin read-out solve is {}", m.status));
		/* Per tag, the candidates' margins in order c0, c1, ... */
		std::map<std::string, std::vector<double>> out;
		for (const auto &[tag, text] : m.sep_margins) {
			std::vector<double> &list = out[tag];
			size_t at = 0;
			while ((at = text.find("margin ", at)) != std::string::npos) {
				at += 7;
				list.push_back(std::strtod(text.c_str() + at, nullptr));
			}
		}
		return out;
	};
	const auto pipeline = margins(r.last_input);
	const auto onexact = margins(exact.points);

	std::map<std::string, double> stretched;
	for (const auto &[tag, amount] : r.last_round.stretched) stretched[tag] += amount;

	const auto norm = [](int x) { return ((x % 8) + 12) % 8 - 4; };
	std::vector<std::string> skipped;
	size_t count = 0, contradicted = 0;
	std::string contradicted_tags;
	std::map<std::pair<int, int>, size_t> hist_all, hist_bad;
	for (const Pair &p : pairs) {
		const auto a = pipeline.find(p.tag);
		const auto e = onexact.find(p.tag);
		if (a == pipeline.end() || e == onexact.end() || a->second.empty() || a->second.size() != e->second.size()) {
			skipped.push_back(p.tag);
			continue;
		}
		const std::vector<PlanarCellWall> &w = p.f->walls;
		const size_t n = w.size();
		const auto turn = [&](size_t from, size_t to) {
			int sum = 0;
			for (size_t k = from; k != to; k = (k + 1) % n) sum += norm(w[(k + 1) % n].heading - w[k].heading);
			return sum;
		};
		const int t1 = turn(p.i, p.j), t2 = turn(p.j, p.i);
		size_t pick = 0;
		for (size_t k = 1; k < a->second.size(); k++) {
			if (a->second[k] > a->second[pick]) pick = k;
		}
		std::string holds;
		for (size_t k = 0; k < e->second.size(); k++) {
			if (e->second[k] >= 0) holds += fmt::format("{}c{}", holds.empty() ? "" : " ", k);
		}
		const auto s = stretched.find(p.tag);
		count++;
		hist_all[{t1, t2}]++;
		if (e->second[pick] < 0) {
			contradicted++;
			contradicted_tags += (contradicted_tags.empty() ? "" : ", ") + p.tag;
			hist_bad[{t1, t2}]++;
		}
		WARN(fmt::format("pair {}: {} | {}; turn i->j {} j->i {}; pipeline pick c{} (margin on exact {}); exact holds [{}]; stretched {}", p.tag, CertWallText(c, w[p.i], &exact.points), CertWallText(c, w[p.j], &exact.points), t1, t2, pick, e->second[pick], holds, s == stretched.end() ? 0.0 : s->second));
	}
	std::string skipped_text;
	for (const std::string &t : skipped) skipped_text += (skipped_text.empty() ? "" : ", ") + t;
	WARN(fmt::format("skipped by the code: {} [{}]", skipped.size(), skipped_text));
	const auto hist_text = [](const std::map<std::pair<int, int>, size_t> &h) {
		std::string s;
		for (const auto &[k, n] : h) s += fmt::format("{}({},{}) x{}", s.empty() ? "" : "; ", k.first, k.second, n);
		return s;
	};
	WARN(fmt::format("summary pairs {} contradicted {}", count, contradicted));
	WARN(fmt::format("summary contradicted tags: {}", contradicted_tags));
	WARN(fmt::format("summary (t1,t2) over contradicted: {}", hist_text(hist_bad)));
	WARN(fmt::format("summary (t1,t2) over all pairs: {}", hist_text(hist_all)));
}

TEST_CASE("planar draw: whole-hub exact keep-apart", "[.][planarcert]")
{
	struct Hub {
		std::string name;
		PlanarShapeVariant v;
		std::function<std::string(const PlanarCellWall &)> wall_text;
	};
	std::vector<Hub> hubs;
	for (const auto &[name, text] : std::vector<std::pair<std::string, std::string>>{{"T1", PlanarTText(1)}, {"T2", PlanarTText(2)}, {"T3", PlanarTText(3)}, {"LL2", PlanarLL2Text()}}) {
		const DrawCase c = ShapeFirst(text, 1);
		hubs.push_back({name, c.v, [c](const PlanarCellWall &w) { return CertWallText(c, w, nullptr); }});
	}
	{
		PlanarShapeVariant flex_c;
		PlanarDrawPins pins;
		if (LoadPinned197(flex_c, pins, true)) {
			DrawCase c;
			c.v = flex_c;
			hubs.push_back({"197", flex_c, [c](const PlanarCellWall &w) { return CertWallText(c, w, nullptr); }});
		} else {
			WARN("whole 197: skipped, the variant cannot be built as the 197 pipeline test builds it");
		}
	}
	for (const Hub &h : hubs) {
		PlanarDrawPipelineOptions po;
		po.final_seconds = 5;
		const PlanarDrawPipelineResult pr = DrawPlanarPipeline(h.v, po);
		int largest = 0;
		for (const auto &track : pr.last_round.points) {
			for (const auto &p : track) largest = std::max({largest, p[0], p[1]});
		}
		const double bound = (2 * largest + 15) / 16 * 16;
		PlanarDrawOptions o;
		o.clearance = true;
		o.clear_rows = true;
		o.pairwise = true;
		o.pairwise_all_faces = true;
		o.corridor = true;
		o.tidy = false;
		o.continuous = {"point", "axis", "diagonal", "param", "cross"};
		o.coord_max = bound;
		const PlanarDrawResult r = CheckPlanarDrawable(h.v, 15.0, o);
		WARN(fmt::format("whole {} B {}: {} ms {:.0f} binaries {}", h.name, bound, r.status, r.ms, r.binaries));
		if (r.status == "Infeasible") {
			const std::vector<PlanarFaceWalls> face_walls = PlanarShapeFaceWalls(h.v);
			size_t shown = 0;
			for (const std::string &tag : r.conflict) {
				if (shown++ >= 40) break;
				std::istringstream words(tag);
				std::string family, text = tag;
				uint32_t face = 0, i = 0, j = 0;
				words >> family;
				if (family == "sep" && (words >> face >> i >> j)) {
					for (const PlanarFaceWalls &f : face_walls) {
						if (f.face != face || i >= f.walls.size() || j >= f.walls.size()) continue;
						text = fmt::format("{} | {} | {}", tag, h.wall_text(f.walls[i]), h.wall_text(f.walls[j]));
						break;
					}
				}
				WARN(fmt::format("whole {} conflict {}", h.name, text));
			}
		}
		if (r.status != "Optimal") continue;
		int x0 = INT32_MAX, x1 = INT32_MIN, y0 = INT32_MAX, y1 = INT32_MIN;
		for (const auto &track : r.points) {
			for (const auto &p : track) {
				x0 = std::min(x0, p[0]); x1 = std::max(x1, p[0]);
				y0 = std::min(y0, p[1]); y1 = std::max(y1, p[1]);
			}
		}
		double length = 0;
		for (const auto &track : r.lengths) {
			for (const double l : track) length += l;
		}
		WARN(fmt::format("whole {} box {} x {}, sum of piece lengths {:.1f}", h.name, x1 - x0, y1 - y0, length));
	}
}

TEST_CASE("planar draw: the simplest drawing on every hub", "[.][planaranyhub]")
{
	struct Hub {
		std::string name;
		std::vector<PlanarShapeVariant> variants;
	};
	std::vector<Hub> hubs;
	const std::vector<std::pair<std::string, std::string>> texts = {{"T1", PlanarTText(1)}, {"T2", PlanarTText(2)}, {"T3", PlanarTText(3)}, {"LL2", PlanarLL2Text()}};
	for (const auto &[name, text] : texts) {
		Hub hub;
		hub.name = name;
		std::string error;
		const PlanarTopology topology = ParsePlanarTopology(text, &error);
		REQUIRE(error.empty());
		const JunctionSpec spec = MakeTestJunctionSpec();
		const PlanarStructureResult sr = BuildPlanarStructure(topology, spec, 8);
		for (size_t si = 0; si < sr.variants.size(); si++) {
			const PlanarStructureVariant &sv = sr.variants[si];
			PlanarShapeVariant shaped;
			std::string why;
			if (ShapePlanarStructureVariant(sv, shaped, why)) {
				hub.variants.push_back(std::move(shaped));
			} else {
				WARN(fmt::format("anyhub {} structure variant {}: not shaped: {}", name, si, why));
			}
		}
		hubs.push_back(std::move(hub));
	}
	{
		Hub hub;
		hub.name = "197";
		PlanarShapeVariant shaped;
		PlanarDrawPins pins;
		if (LoadPinned197(shaped, pins, true)) {
			hub.variants.push_back(std::move(shaped));
		} else {
			WARN("anyhub 197: not loaded");
		}
		hubs.push_back(std::move(hub));
	}

	const auto report = [](const std::string &head, const PlanarDrawResult &r) {
		WARN(fmt::format("anyhub {}: {} slack {:.3f} ms {:.0f}", head, r.status, r.slack, r.ms));
		std::map<std::string, std::pair<size_t, double>> families;
		for (const auto &[tag, slack] : r.stretched) {
			auto &f = families[tag.substr(0, tag.find(' '))];
			f.first++;
			f.second += slack;
		}
		std::string line;
		for (const auto &[family, f] : families) line += fmt::format(" [{} {} {:.3f}]", family, f.first, f.second);
		if (!line.empty()) WARN(fmt::format("anyhub {} families:{}", head, line));
		line.clear();
		for (size_t i = 0; i < std::min<size_t>(5, r.stretched.size()); i++) line += fmt::format(" [{} {:.3f}]", r.stretched[i].first, r.stretched[i].second);
		if (!line.empty()) WARN(fmt::format("anyhub {} largest:{}", head, line));
	};
	const char *dump_root = std::getenv("PLANAR_DUMP");

	for (const Hub &hub : hubs) {
		std::array<size_t, 2> zero{}, run{}, integer{};
		for (size_t vi = 0; vi < hub.variants.size(); vi++) {
			const PlanarShapeVariant &v = hub.variants[vi];
			const std::string tag = fmt::format("v{}", vi);
			std::vector<std::vector<double>> zeros;
			for (const PlanarShapeTrack &track : v.tracks) zeros.emplace_back(track.pieces.size(), 0.0);
			for (int k = 0; k < 2; k++) {
				const std::string kname = k == 0 ? "K1" : "K2";
				PlanarDrawOptions o;
				o.clearance = true;
				o.pairwise = false;
				o.corridor = true;
				o.elastic = true;
				if (k == 1) {
					o.flex_zero_from = &zeros;
					o.flex_zero_below = 1;
				}
				if (dump_root != nullptr && vi == 0) {
					std::filesystem::create_directories(fmt::format("{}/anyhub", dump_root));
					o.dump_path = fmt::format("{}/anyhub/{}-{}.json", dump_root, hub.name, kname);
				}
				const PlanarDrawResult r = CheckPlanarDrawable(v, 15, o);
				report(fmt::format("{} {} {}", hub.name, tag, kname), r);
				run[k]++;
				const bool flat = r.status == "Optimal" && r.slack < 1e-6;
				if (flat) {
					zero[k]++;
					o.elastic = false;
					o.dump_path.clear();
					const PlanarDrawResult ri = CheckPlanarDrawable(v, 15, o);
					WARN(fmt::format("anyhub {} {} {} K4: {} ms {:.0f}", hub.name, tag, kname, ri.status, ri.ms));
					if (ri.status == "Optimal") integer[k]++;
				}
			}
			/* PLANAR_ANYHUB_K3_ALL (a measurement aid): the pipeline on every variant, not only the first. */
			if (vi == 0 || std::getenv("PLANAR_ANYHUB_K3_ALL") != nullptr) {
				PlanarDrawPipelineOptions po;
				po.final_seconds = 5;
				/* PLANAR_ANYHUB_GRID (a measurement aid): the integer final on the tile grid, 15 s. */
				if (std::getenv("PLANAR_ANYHUB_GRID") != nullptr) {
					po.final_grid = true;
					po.final_seconds = 15;
				}
				po.final_contract = std::getenv("PLANAR_ANYHUB_CONTRACT") != nullptr;
				const PlanarDrawPipelineResult pr = DrawPlanarPipeline(v, po);
				WARN(fmt::format("anyhub {} {} K3: last round {} slack {:.3f}; final {} ms {:.0f}; stopped '{}'; pairs {} contracted {} steps {}", hub.name, tag, pr.last_round.status, pr.last_round.slack, pr.final.status, pr.final.ms, pr.stopped, pr.final_pairs, pr.contracted, pr.contract_steps));
				if (std::getenv("PLANAR_ANYHUB_CONFLICT") != nullptr) for (const std::string &t : pr.final.conflict) WARN(fmt::format("anyhub {} {} K3 conflict {}", hub.name, tag, t));
			}
		}
		for (int k = 0; k < 2; k++) WARN(fmt::format("anyhub summary {} {}: slack 0 {}/{}, integer Optimal {}", hub.name, k == 0 ? "K1" : "K2", zero[k], run[k], integer[k]));
	}
}
