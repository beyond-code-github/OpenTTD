/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_hub.cpp The drawn hub: the contract from Slots to the back end (D452, SPEC-slots-lengths section 4.1). */

#include "../../stdafx.h"

#include "planar_hub.h"
#include "planar_domain_map.h"

#include "../../core/format.hpp"

#include <algorithm>
#include <climits>

#include "../../safeguards.h"

namespace {

/** The bounding box of the tiles every legal track's raster occupies. */
struct TileBox {
	int min_x = INT_MAX;
	int min_y = INT_MAX;
	int max_x = INT_MIN;
	int max_y = INT_MIN;
	bool Empty() const { return this->max_x < this->min_x; }
};

TileBox BoxOf(const std::vector<std::vector<std::array<int, 2>>> &tracks)
{
	TileBox box;
	for (const auto &points : tracks) {
		for (const PlanarDomainPiece &piece : RasterPlanarDomainTrack(points).pieces) {
			box.min_x = std::min<int>(box.min_x, piece.tile.x);
			box.min_y = std::min<int>(box.min_y, piece.tile.y);
			box.max_x = std::max<int>(box.max_x, piece.tile.x);
			box.max_y = std::max<int>(box.max_y, piece.tile.y);
		}
	}
	return box;
}

}

PlanarDrawnHub PlanarDrawnHubOf(const PlanarShapeVariant &v, const std::vector<std::vector<std::array<int, 2>>> &points, std::vector<std::string> &notes, std::string &why)
{
	const PlanarTrackMap &map = v.structure.map;
	const PlanarDomainInput in = PlanarDomainInputOf(v, points, notes);
	if (in.tracks.size() != map.tracks.size() || map.expanded.lines.size() != map.tracks.size()) {
		why = fmt::format("the domain input has {} tracks, the track map {} (points {})", in.tracks.size(), map.tracks.size(), points.size());
		return {};
	}
	for (size_t i = 0; i < map.tracks.size(); i++) {
		if (map.tracks[i].kind == PlanarTrack::Kind::Line && in.tracks[i] != points[i]) {
			why = fmt::format("domain track {} is not line {}", i, map.expanded.lines[i].name);
			return {};
		}
	}
	PlanarDrawnHub hub;
	for (size_t i = 0; i < map.tracks.size(); i++) hub.tracks.push_back({map.tracks[i].name, map.tracks[i].kind, in.tracks[i]});
	const auto shared = [](const std::vector<uint32_t> &a, const std::vector<uint32_t> &b) {
		return std::ranges::any_of(a, [&](uint32_t t) { return std::ranges::find(b, t) != b.end(); });
	};
	for (const PlanarDomainNode &n : in.nodes) {
		bool split = false;
		for (const PlanarNode &e : map.expanded.nodes) {
			if (e.IsSplit() && (shared(e.in, n.in) || shared(e.out, n.out))) split = true;
		}
		hub.nodes.push_back({n.at, n.in, n.out, split});
	}
	for (const PlanarPort &p : map.expanded.ports) hub.ports.push_back({p.side, p.in, p.lines});
	for (const PlanarDomainStructure &s : in.structures) hub.structures.push_back({s.separator, s.first, s.last, s.ends, s.tunnel, s.crossers});
	for (const PlanarRoute &r : map.expanded.routes) hub.routes.push_back({r.name, r.prio, r.paths});

	/* Whole tiles: shift every point by one even vector so the least tile is (0, 0). */
	std::vector<std::vector<std::array<int, 2>>> all;
	for (const PlanarDrawnHub::Track &t : hub.tracks) all.push_back(t.points);
	const TileBox box = BoxOf(all);
	if (!box.Empty()) {
		for (PlanarDrawnHub::Track &t : hub.tracks) {
			for (std::array<int, 2> &p : t.points) {
				p[0] -= 2 * box.min_x;
				p[1] -= 2 * box.min_y;
			}
		}
		for (PlanarDrawnHub::Node &n : hub.nodes) {
			n.at[0] -= 2 * box.min_x;
			n.at[1] -= 2 * box.min_y;
		}
	}
	return hub;
}

PlanarDomainInput PlanarDomainInputOfHub(const PlanarDrawnHub &hub)
{
	PlanarDomainInput in;
	for (const PlanarDrawnHub::Track &t : hub.tracks) in.tracks.push_back(t.points);
	for (const PlanarDrawnHub::Node &n : hub.nodes) in.nodes.push_back({n.at, n.in, n.out});
	for (const PlanarDrawnHub::Structure &s : hub.structures) {
		PlanarDomainStructure d;
		d.separator = s.separator;
		d.first = s.first;
		d.last = s.last;
		d.tunnel = s.tunnel;
		d.crossers = s.crossers;
		d.ends = s.ends;
		in.structures.push_back(std::move(d));
	}
	return in;
}

std::vector<std::string> CheckPlanarDrawnHub(const PlanarDrawnHub &hub)
{
	std::vector<std::string> lines;
	std::vector<PlanarDomainRaster> rasters;
	std::vector<std::vector<std::array<int, 2>>> all;
	for (const PlanarDrawnHub::Track &t : hub.tracks) {
		rasters.push_back(RasterPlanarDomainTrack(t.points));
		all.push_back(t.points);
		if (!rasters.back().rule.empty()) lines.push_back(fmt::format("track {} does not rasterise: {} ({})", t.name, rasters.back().rule, rasters.back().why));
	}
	const auto name = [&](uint32_t t) { return t < hub.tracks.size() ? hub.tracks[t].name : fmt::format("#{}", t); };

	/* Every path's consecutive tracks meet at a node that lists them (the first break of a path). */
	for (const PlanarDrawnHub::Route &r : hub.routes) {
		for (size_t p = 0; p < r.paths.size(); p++) {
			const std::vector<uint32_t> &path = r.paths[p];
			for (size_t k = 0; k + 1 < path.size(); k++) {
				const bool meet = std::ranges::any_of(hub.nodes, [&](const PlanarDrawnHub::Node &n) {
					return std::ranges::find(n.in, path[k]) != n.in.end() && std::ranges::find(n.out, path[k + 1]) != n.out.end();
				});
				if (!meet) {
					lines.push_back(fmt::format("route {} path {}: tracks {} then {} share no node", r.name, p, name(path[k]), name(path[k + 1])));
					break;
				}
			}
		}
	}

	/* Every port track's port-side end lies on the box edge of its side. */
	const TileBox box = BoxOf(all);
	if (!box.Empty()) {
		for (size_t i = 0; i < hub.ports.size(); i++) {
			const PlanarDrawnHub::Port &port = hub.ports[i];
			for (uint32_t t : port.tracks) {
				if (t >= hub.tracks.size() || hub.tracks[t].points.empty()) continue;
				const std::array<int, 2> end = port.in ? hub.tracks[t].points.front() : hub.tracks[t].points.back();
				int at = 0, edge = 0;
				switch (port.side) {
					case PlanarSide::W: at = end[0]; edge = 2 * box.min_x; break;
					case PlanarSide::E: at = end[0]; edge = 2 * (box.max_x + 1); break;
					case PlanarSide::N: at = end[1]; edge = 2 * box.min_y; break;
					case PlanarSide::S: at = end[1]; edge = 2 * (box.max_y + 1); break;
				}
				if (at != edge) lines.push_back(fmt::format("port {} track {}: its {} end is at ({},{}), off the box edge {}", i, name(t), port.in ? "start" : "finish", end[0], end[1], edge));
			}
		}
	}

	/* Every structure's first..last lies within its separator's raster with axis end pieces. */
	for (size_t k = 0; k < hub.structures.size(); k++) {
		const PlanarDrawnHub::Structure &s = hub.structures[k];
		if (s.separator >= hub.tracks.size()) {
			lines.push_back(fmt::format("structure {}: separator {} is no track", k, s.separator));
			continue;
		}
		const std::vector<PlanarDomainPiece> &pieces = rasters[s.separator].pieces;
		if (s.first > s.last || s.last >= pieces.size()) {
			lines.push_back(fmt::format("structure {} ({}): first {} last {} outside its {} pieces", k, name(s.separator), s.first, s.last, pieces.size()));
			continue;
		}
		bool axis = true;
		for (int e = 0; e < std::max(1, s.ends); e++) {
			axis = axis && pieces[s.first + e].heading % 2 == 0 && pieces[s.last - e].heading % 2 == 0;
		}
		if (!axis) lines.push_back(fmt::format("structure {} ({}): an end piece is not on an axis", k, name(s.separator)));
	}

	/* The domain judge accepts. */
	for (const PlanarDomainBreak &b : CheckPlanarDomain(PlanarDomainInputOfHub(hub)).breaks) {
		std::string names;
		for (uint32_t t : b.tracks) names += (names.empty() ? "" : ",") + name(t);
		lines.push_back(fmt::format("domain {} at {},{} ({}): {}", b.rule, b.tile.x, b.tile.y, names, b.why));
	}
	return lines;
}
