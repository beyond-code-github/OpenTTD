/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_domain_map.cpp A drawing of a shaped variant as the domain's input (D428). */

#include "../../stdafx.h"

#include "planar_domain_map.h"
#include "planar_draw.h"
#include "planar_shape_layer.h"

#include "../../core/format.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <tuple>

#include "../../safeguards.h"

using DomainPoints = std::vector<std::array<int, 2>>;

/** Whether every point of a track is the same (or it has none). */
static bool IsZeroLength(const DomainPoints &track)
{
	return std::all_of(track.begin(), track.end(), [&](const std::array<int, 2> &p) { return p == track.front(); });
}

/** Append the values of from not yet in to. */
static void Union(std::vector<uint32_t> &to, const std::vector<uint32_t> &from)
{
	for (uint32_t t : from) {
		if (std::find(to.begin(), to.end(), t) == to.end()) to.push_back(t);
	}
}

/** The nodes of the input: expanded nodes at one point merged, zero-length tracks dropped. */
static std::vector<PlanarDomainNode> NodesOf(const PlanarShapeVariant &v, const std::vector<DomainPoints> &points)
{
	std::vector<PlanarDomainNode> nodes;
	for (const PlanarNode &n : v.structure.map.expanded.nodes) {
		bool found = false;
		std::array<int, 2> at{};
		for (uint32_t t : n.in) {
			if (t < points.size() && !points[t].empty()) {
				at = points[t].back();
				found = true;
				break;
			}
		}
		if (!found) continue;
		auto it = std::find_if(nodes.begin(), nodes.end(), [&](const PlanarDomainNode &m) { return m.at == at; });
		if (it == nodes.end()) {
			nodes.push_back({at, {}, {}});
			it = nodes.end() - 1;
		}
		Union(it->in, n.in);
		Union(it->out, n.out);
	}
	for (PlanarDomainNode &n : nodes) {
		auto zero = [&](uint32_t t) { return t < points.size() && IsZeroLength(points[t]); };
		n.in.erase(std::remove_if(n.in.begin(), n.in.end(), zero), n.in.end());
		n.out.erase(std::remove_if(n.out.begin(), n.out.end(), zero), n.out.end());
	}
	return nodes;
}

/** Add the structure on one separator to the input, or a note why it is left out. */
static void AddStructure(const PlanarShapeVariant &v, PlanarDomainInput &input, std::vector<std::string> &notes, uint32_t k, const PlanarDomainStructure &s)
{
	const std::string head = fmt::format("structure {} ({})", k, v.structure.map.expanded.lines[s.separator].name);
	const PlanarDomainRaster sep = RasterPlanarDomainTrack(input.tracks[s.separator]);
	if (!sep.rule.empty()) {
		notes.push_back(fmt::format("{}: raster: {}", head, sep.rule));
		return;
	}
	std::vector<bool> met(sep.pieces.size(), false);
	for (uint32_t c : s.crossers) {
		const PlanarDomainRaster cr = RasterPlanarDomainTrack(input.tracks[c]);
		if (!cr.rule.empty()) {
			notes.push_back(fmt::format("{}: raster: {}", head, cr.rule));
			return;
		}
		for (size_t i = 0; i < sep.pieces.size(); i++) {
			for (const PlanarDomainPiece &p : cr.pieces) {
				if (p.tile.x == sep.pieces[i].tile.x && p.tile.y == sep.pieces[i].tile.y) met[i] = true;
			}
		}
	}
	const auto lo = std::find(met.begin(), met.end(), true);
	if (lo == met.end()) {
		notes.push_back(fmt::format("{}: no crosser meets its separator", head));
		return;
	}
	const size_t least = lo - met.begin();
	const size_t greatest = met.size() - 1 - (std::find(met.rbegin(), met.rend(), true) - met.rbegin());
	if (least < static_cast<size_t>(s.ends) || greatest + s.ends >= met.size()) {
		notes.push_back(fmt::format("{}: no room for its end runs", head));
		return;
	}
	PlanarDomainStructure out = s;
	out.first = static_cast<uint32_t>(least - s.ends);
	out.last = static_cast<uint32_t>(greatest + s.ends);
	input.structures.push_back(std::move(out));
}

PlanarDomainInput PlanarDomainInputOf(const PlanarShapeVariant &v, const std::vector<std::vector<std::array<int, 2>>> &points, std::vector<std::string> &notes)
{
	PlanarDomainInput input;
	if (points.size() != v.structure.map.expanded.lines.size()) {
		notes.push_back("points do not match the variant");
		return input;
	}
	input.tracks = points;
	/* A beside track (a copy or a prio row, D421): the drawing leaves its opening and closing steps implicit, so the domain input adds them: the step from its from-node's point to its first point and from its last point to its to-node's point. */
	const PlanarTrackMap &map = v.structure.map;
	for (uint32_t b = 0; b < map.tracks.size(); b++) {
		if (map.part_of[b] != UINT32_MAX || input.tracks[b].empty() || v.tracks[b].pieces.empty()) continue;
		std::vector<uint32_t> chain;
		if (!ParentChain(map, v.structure.sites, b, chain)) continue;
		const int side = map.Side(b, 0, chain[0], 0);
		if (side == 0) continue;
		const std::array<int, 2> opening = PlanarBesideOpening(v.tracks[b].pieces.front(), side);
		const std::array<int, 2> closing = PlanarBesideClosing(v.tracks[b].pieces.back(), side);
		DomainPoints &track = input.tracks[b];
		const std::array<int, 2> first = track.front(), last = track.back();
		track.insert(track.begin(), {first[0] - opening[0], first[1] - opening[1]});
		track.push_back({last[0] + closing[0], last[1] + closing[1]});
	}
	input.nodes = NodesOf(v, input.tracks);

	uint32_t k = 0;
	for (const PlanarStructure &s : v.structure.structures) {
		const bool tunnel = (s.kinds & 1) == 0;
		PlanarDomainStructure d;
		d.separator = s.separator;
		d.crossers = s.crossers;
		d.tunnel = tunnel;
		d.limit = s.limit[tunnel ? 1 : 0];
		d.first = d.last = 0;
		d.ends = v.structures[k].end_run[tunnel ? 1 : 0];
		AddStructure(v, input, notes, k, d);
		if (s.copy != UINT32_MAX) {
			PlanarDomainStructure c = d;
			c.separator = s.copy;
			c.crossers.clear();
			for (const PlanarCrossing &x : v.structure.map.crossings) {
				if (x.a == s.copy) c.crossers.push_back(x.b);
				else if (x.b == s.copy) c.crossers.push_back(x.a);
			}
			AddStructure(v, input, notes, k, c);
		}
		k++;
	}
	return input;
}
