/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file planar_topology.cpp The planar junction topology implementation (DESIGN-planar §1). */

#include "../../stdafx.h"

#include "planar_topology.h"

#include "../../3rdparty/fmt/format.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <set>

#include "../../safeguards.h"

/** Strip blanks at both ends. */
static std::string_view Trim(std::string_view s)
{
	while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1);
	while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
	return s;
}

/** Split on commas, trimmed, empty items dropped. */
static std::vector<std::string> SplitList(std::string_view s)
{
	std::vector<std::string> out;
	while (!s.empty()) {
		const size_t comma = s.find(',');
		const std::string_view item = Trim(s.substr(0, comma));
		if (!item.empty()) out.emplace_back(item);
		if (comma == std::string_view::npos) break;
		s.remove_prefix(comma + 1);
	}
	return out;
}

/** The unit vector of a heading, y to the south. */
static std::pair<int, int> HeadingVector(PlanarHeading h)
{
	static const std::pair<int, int> v[] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};
	return v[static_cast<int>(h)];
}

/** The parser's and deriver's state. */
struct PlanarBuilder {
	PlanarTopology t;
	std::map<std::string, uint32_t, std::less<>> line_index;
	std::map<std::string, uint32_t, std::less<>> route_index;
	std::string error;

	/** The route of a name, created on first use. */
	uint32_t Route(const std::string &name)
	{
		auto it = this->route_index.find(name);
		if (it != this->route_index.end()) return it->second;
		const uint32_t r = static_cast<uint32_t>(this->t.routes.size());
		this->t.routes.push_back({name, false, {}});
		this->route_index.emplace(name, r);
		return r;
	}

	/** The line of a name, created on first use with its routes from the notation. */
	uint32_t Line(const std::string &spelled)
	{
		std::string name = spelled;
		std::vector<std::string> routes;
		const size_t eq = spelled.find('=');
		if (eq != std::string::npos) {
			name = std::string(Trim(std::string_view(spelled).substr(0, eq)));
			std::string_view rest = std::string_view(spelled).substr(eq + 1);
			while (!rest.empty()) {
				const size_t plus = rest.find('+');
				const std::string_view r = Trim(rest.substr(0, plus));
				if (!r.empty()) routes.emplace_back(r);
				if (plus == std::string_view::npos) break;
				rest.remove_prefix(plus + 1);
			}
		}
		auto it = this->line_index.find(name);
		if (it != this->line_index.end()) return it->second;
		if (routes.empty()) {
			size_t i = 0;
			while (i < name.size() && name[i] >= 'A' && name[i] <= 'Z') i++;
			size_t j = i;
			while (j < name.size() && name[j] >= '0' && name[j] <= '9') j++;
			size_t k = j;
			while (k < name.size() && name[k] >= 'a' && name[k] <= 'z') k++;
			if (i == 0 || j == i || k != name.size()) {
				if (this->error.empty()) this->error = fmt::format("line name '{}' is not letters, a lane and a suffix", name);
				return UINT32_MAX;
			}
			for (size_t c = 0; c < i; c++) routes.push_back(name.substr(c, 1) + name.substr(i, j - i));
		}
		PlanarLine line;
		line.name = name;
		for (const std::string &r : routes) line.routes.push_back(this->Route(r));
		std::ranges::sort(line.routes);
		line.routes.erase(std::unique(line.routes.begin(), line.routes.end()), line.routes.end());
		const uint32_t l = static_cast<uint32_t>(this->t.lines.size());
		this->t.lines.push_back(std::move(line));
		this->line_index.emplace(name, l);
		return l;
	}

	/** Set one end of a line; an end set twice is an error. */
	void SetEnd(uint32_t l, bool from, PlanarEnd end)
	{
		if (l == UINT32_MAX) return;
		PlanarEnd &e = from ? this->t.lines[l].from : this->t.lines[l].to;
		if (e.index != UINT32_MAX) {
			if (this->error.empty()) this->error = fmt::format("line {} has two {} ends", this->t.lines[l].name, from ? "entry" : "exit");
			return;
		}
		e = end;
	}

	void ParsePort(std::string_view rest);
	void ParseNode(std::string_view rest);
	void Derive();
};

void PlanarBuilder::ParsePort(std::string_view rest)
{
	const size_t colon = rest.find(':');
	if (colon == std::string_view::npos) {
		this->error = "a port needs ':' before its lines";
		return;
	}
	std::vector<std::string> words;
	std::string_view head = Trim(rest.substr(0, colon));
	while (!head.empty()) {
		const size_t sp = head.find(' ');
		words.emplace_back(head.substr(0, sp));
		if (sp == std::string_view::npos) break;
		head = Trim(head.substr(sp + 1));
	}
	static const std::map<std::string, PlanarSide, std::less<>> sides = {{"N", PlanarSide::N}, {"E", PlanarSide::E}, {"S", PlanarSide::S}, {"W", PlanarSide::W}};
	static const std::map<std::string, PlanarHeading, std::less<>> headings = {{"N", PlanarHeading::N}, {"NE", PlanarHeading::NE}, {"E", PlanarHeading::E}, {"SE", PlanarHeading::SE}, {"S", PlanarHeading::S}, {"SW", PlanarHeading::SW}, {"W", PlanarHeading::W}, {"NW", PlanarHeading::NW}};
	if (words.size() != 3 || !sides.contains(words[0]) || (words[1] != "in" && words[1] != "out") || !headings.contains(words[2])) {
		this->error = fmt::format("a port is 'port <N|E|S|W> <in|out> <heading>: lines', not '{}'", rest);
		return;
	}
	PlanarPort port{sides.find(words[0])->second, words[1] == "in", headings.find(words[2])->second, {}};
	const uint32_t p = static_cast<uint32_t>(this->t.ports.size());
	for (const std::string &name : SplitList(rest.substr(colon + 1))) {
		const uint32_t l = this->Line(name);
		port.lines.push_back(l);
		this->SetEnd(l, port.in, {true, p});
	}
	if (port.lines.empty() && this->error.empty()) this->error = "a port has no lines";
	this->t.ports.push_back(std::move(port));
}

void PlanarBuilder::ParseNode(std::string_view rest)
{
	const size_t arrow = rest.find("->");
	if (arrow == std::string_view::npos) {
		this->error = "a node needs '->'";
		return;
	}
	std::vector<uint32_t> in, out;
	for (const std::string &n : SplitList(rest.substr(0, arrow))) in.push_back(this->Line(n));
	for (const std::string &n : SplitList(rest.substr(arrow + 2))) out.push_back(this->Line(n));
	if (!this->error.empty()) return;
	const uint32_t group = static_cast<uint32_t>(std::ranges::count_if(this->t.nodes, [](const PlanarNode &) { return true; }));
	const auto subset = [&](uint32_t small, uint32_t big) { return std::ranges::includes(this->t.lines[big].routes, this->t.lines[small].routes); };
	const auto add = [&](std::vector<uint32_t> node_in, std::vector<uint32_t> node_out) {
		const uint32_t n = static_cast<uint32_t>(this->t.nodes.size());
		for (uint32_t l : node_in) this->SetEnd(l, false, {false, n});
		for (uint32_t l : node_out) this->SetEnd(l, true, {false, n});
		this->t.nodes.push_back({std::move(node_in), std::move(node_out), group});
	};
	if (in.size() < out.size()) {
		/* Splits: every out line belongs to the one in line whose routes contain its own; each in line gets exactly two, in authored order. */
		for (uint32_t i : in) {
			std::vector<uint32_t> legs;
			for (uint32_t o : out) if (subset(o, i)) legs.push_back(o);
			if (legs.size() != 2) {
				this->error = fmt::format("node at {}: every split is 1 -> 2, {} has {} legs", this->t.lines[i].name, this->t.lines[i].name, legs.size());
				return;
			}
			add({i}, legs);
		}
		if (out.size() != 2 * in.size()) this->error = fmt::format("node at {}: an out line belongs to no in line or to two", this->t.lines[in.front()].name);
	} else if (in.size() > out.size()) {
		/* Merges: every out line joins the two in lines whose routes it contains. */
		for (uint32_t o : out) {
			std::vector<uint32_t> legs;
			for (uint32_t i : in) if (subset(i, o)) legs.push_back(i);
			if (legs.size() != 2) {
				this->error = fmt::format("node at {}: every merge is 2 -> 1, {} has {} legs", this->t.lines[o].name, this->t.lines[o].name, legs.size());
				return;
			}
			add(legs, {o});
		}
		if (in.size() != 2 * out.size()) this->error = fmt::format("node at {}: an in line joins no out line or two", this->t.lines[out.front()].name);
	} else {
		this->error = fmt::format("node at {}: every split is 1 -> 2 and every merge 2 -> 1", in.empty() ? std::string("?") : this->t.lines[in.front()].name);
	}
}

void PlanarBuilder::Derive()
{
	PlanarTopology &t = this->t;
	for (const PlanarLine &line : t.lines) {
		if (line.from.index == UINT32_MAX || line.to.index == UINT32_MAX) {
			this->error = fmt::format("line {} has no {} end", line.name, line.from.index == UINT32_MAX ? "entry" : "exit");
			return;
		}
		if (line.from.port && line.to.port) {
			this->error = fmt::format("line {} runs from port to port with no node", line.name);
			return;
		}
	}
	/* The ring: each side's ports in reading order (N, S west to east; W, E north to south), each port's lines in reading order by travel-left; then clockwise. */
	std::map<PlanarSide, std::vector<uint32_t>> side_lines;
	for (const PlanarPort &port : t.ports) {
		const auto [dx, dy] = HeadingVector(port.heading);
		const bool along_x = port.side == PlanarSide::N || port.side == PlanarSide::S;
		const int right_along = along_x ? -dy : dx; // right of travel (-dy, dx) dotted with the reading direction
		if (right_along == 0) {
			this->error = fmt::format("port of {} heads along its side", t.lines[port.lines.front()].name);
			return;
		}
		std::vector<uint32_t> lines = port.lines;
		if (right_along < 0) std::ranges::reverse(lines);
		std::vector<uint32_t> &v = side_lines[port.side];
		v.insert(v.end(), lines.begin(), lines.end());
	}
	for (PlanarSide side : {PlanarSide::N, PlanarSide::E, PlanarSide::S, PlanarSide::W}) {
		std::vector<uint32_t> v = side_lines[side];
		if (side == PlanarSide::S || side == PlanarSide::W) std::ranges::reverse(v);
		t.ring.insert(t.ring.end(), v.begin(), v.end());
	}
	std::vector<int> ring_pos(t.lines.size(), -1);
	for (size_t i = 0; i < t.ring.size(); i++) ring_pos[t.ring[i]] = static_cast<int>(i);

	/* Paths: per route, every walk over lines carrying it from an entry port line to an exit port line. */
	struct Path { uint32_t route; std::vector<uint32_t> lines; };
	std::vector<Path> paths;
	for (uint32_t r = 0; r < t.routes.size(); r++) {
		const auto carries = [&](uint32_t l) { return std::ranges::binary_search(t.lines[l].routes, r); };
		std::vector<std::vector<uint32_t>> stack;
		for (uint32_t l = 0; l < t.lines.size(); l++) {
			if (carries(l) && t.lines[l].from.port) stack.push_back({l});
		}
		if (stack.size() != 1) {
			this->error = fmt::format("route {} has {} entry lines, not one", t.routes[r].name, stack.size());
			return;
		}
		while (!stack.empty()) {
			std::vector<uint32_t> walk = std::move(stack.back());
			stack.pop_back();
			const PlanarEnd to = t.lines[walk.back()].to;
			if (to.port) {
				if (t.ports[to.index].in) {
					this->error = fmt::format("line {} leaves through an in port", t.lines[walk.back()].name);
					return;
				}
				t.routes[r].paths.push_back(walk);
				continue;
			}
			std::vector<uint32_t> next;
			for (uint32_t l : t.nodes[to.index].out) if (carries(l)) next.push_back(l);
			if (next.empty()) {
				this->error = fmt::format("route {} ends at a node after {}", t.routes[r].name, t.lines[walk.back()].name);
				return;
			}
			for (auto it = next.rbegin(); it != next.rend(); ++it) {
				std::vector<uint32_t> w = walk;
				w.push_back(*it);
				stack.push_back(std::move(w));
			}
		}
		for (const std::vector<uint32_t> &w : t.routes[r].paths) paths.push_back({r, w});
	}

	/* Interleaving: two paths of different routes sharing no line cross iff their ends interleave around the ring. */
	const auto interleave = [&](const Path &p, const Path &q) {
		if (p.route == q.route) return false;
		for (uint32_t l : p.lines) if (std::ranges::find(q.lines, l) != q.lines.end()) return false;
		const int a0 = ring_pos[p.lines.front()], a1 = ring_pos[p.lines.back()];
		const int lo = std::min(a0, a1), hi = std::max(a0, a1);
		const auto inside = [&](int x) { return lo < x && x < hi; };
		return inside(ring_pos[q.lines.front()]) != inside(ring_pos[q.lines.back()]);
	};
	std::set<std::pair<size_t, size_t>> must;
	for (size_t i = 0; i < paths.size(); i++) {
		for (size_t j = i + 1; j < paths.size(); j++) if (interleave(paths[i], paths[j])) must.insert({i, j});
	}

	/* Line pairs: a crossing crosses every path through one line with every path through the other, so it is admissible when all those pairs interleave. Port lines are the stubs at the box edge and never cross. */
	struct Candidate { uint32_t a, b; std::vector<std::pair<size_t, size_t>> covers; };
	std::vector<Candidate> candidates;
	const auto port_line = [&](uint32_t l) { return t.lines[l].from.port || t.lines[l].to.port; };
	for (uint32_t a = 0; a < t.lines.size(); a++) {
		if (port_line(a)) continue;
		for (uint32_t b = a + 1; b < t.lines.size(); b++) {
			if (port_line(b)) continue;
			Candidate c{a, b, {}};
			bool ok = true;
			const auto through = [&](size_t i, uint32_t l) { return std::ranges::find(paths[i].lines, l) != paths[i].lines.end(); };
			for (size_t i = 0; i < paths.size() && ok; i++) {
				if (!through(i, a)) continue;
				for (size_t j = 0; j < paths.size() && ok; j++) {
					if (!through(j, b)) continue;
					const std::pair<size_t, size_t> key{std::min(i, j), std::max(i, j)};
					if (i == j || !must.contains(key)) {
						ok = false;
					} else {
						c.covers.push_back(key);
					}
				}
			}
			if (ok && !c.covers.empty()) candidates.push_back(std::move(c));
		}
	}
	/* The admissible pairs covering the most path pairs first (a bundle line crosses whole), then by line index. */
	std::ranges::stable_sort(candidates, [](const Candidate &x, const Candidate &y) { return x.covers.size() > y.covers.size(); });
	std::set<std::pair<size_t, size_t>> covered;
	for (const Candidate &c : candidates) {
		if (std::ranges::any_of(c.covers, [&](const auto &k) { return covered.contains(k); })) continue;
		for (const auto &k : c.covers) covered.insert(k);
		t.crossings.push_back({c.a, c.b});
	}
	if (covered.size() != must.size()) {
		for (const auto &k : must) {
			if (covered.contains(k)) continue;
			this->error = fmt::format("routes {} and {} must cross but no two of their separate lines can", t.routes[paths[k.first].route].name, t.routes[paths[k.second].route].name);
			return;
		}
	}
	std::ranges::sort(t.crossings, [](const PlanarCrossing &x, const PlanarCrossing &y) { return std::pair(x.a, x.b) < std::pair(y.a, y.b); });

	/* Event order: each line's crossings in the order a straight chord of its first path meets the chords of the other lines' first paths (a stretchable arrangement, so the orders agree).
	 * The ring's ports stand on a circle at slightly uneven angles (no three chords meet in a point), each port's lines packed close together, so a bundle crosses another whole (D306). */
	std::vector<std::pair<uint32_t, uint32_t>> ring_port(t.ring.size()); // (port rank around the ring, line index within the port)
	{
		const auto port_of = [&](uint32_t l) { return t.lines[l].from.port ? t.lines[l].from.index : t.lines[l].to.index; };
		for (size_t i = 0; i < t.ring.size(); i++) {
			if (i == 0) continue;
			const bool same = port_of(t.ring[i]) == port_of(t.ring[i - 1]);
			ring_port[i] = same ? std::pair(ring_port[i - 1].first, ring_port[i - 1].second + 1) : std::pair(ring_port[i - 1].first + 1, 0u);
		}
	}
	const double ports_n = ring_port.empty() ? 1.0 : ring_port.back().first + 1.0;
	const auto point = [&](int i) {
		const auto [rank, k] = ring_port[i];
		const double angle = 2 * std::numbers::pi * (rank + 0.15 * std::sqrt(rank + 2.0) + 0.02 * k) / ports_n;
		return std::pair<double, double>{std::cos(angle), std::sin(angle)};
	};
	std::vector<size_t> rep(t.lines.size(), SIZE_MAX);
	for (size_t i = 0; i < paths.size(); i++) {
		for (uint32_t l : paths[i].lines) if (rep[l] == SIZE_MAX) rep[l] = i;
	}
	const auto param = [&](uint32_t l, uint32_t m) {
		const auto [p0x, p0y] = point(ring_pos[paths[rep[l]].lines.front()]);
		const auto [p1x, p1y] = point(ring_pos[paths[rep[l]].lines.back()]);
		const auto [q0x, q0y] = point(ring_pos[paths[rep[m]].lines.front()]);
		const auto [q1x, q1y] = point(ring_pos[paths[rep[m]].lines.back()]);
		const double rx = p1x - p0x, ry = p1y - p0y, sx = q1x - q0x, sy = q1y - q0y;
		const double den = rx * sy - ry * sx;
		return ((q0x - p0x) * sy - (q0y - p0y) * sx) / den;
	};
	t.events.assign(t.lines.size(), {});
	for (uint32_t l = 0; l < t.lines.size(); l++) {
		std::vector<std::pair<double, uint32_t>> along;
		for (uint32_t c = 0; c < t.crossings.size(); c++) {
			if (t.crossings[c].a != l && t.crossings[c].b != l) continue;
			along.push_back({param(l, t.crossings[c].a == l ? t.crossings[c].b : t.crossings[c].a), c});
		}
		std::ranges::sort(along);
		for (const auto &[_, c] : along) t.events[l].push_back(c);
	}
}

PlanarTopology ParsePlanarTopology(std::string_view text, std::string *error)
{
	PlanarBuilder b;
	std::vector<std::string> prio;
	while (!text.empty() && b.error.empty()) {
		const size_t nl = text.find('\n');
		std::string_view line = text.substr(0, nl);
		text.remove_prefix(nl == std::string_view::npos ? text.size() : nl + 1);
		const size_t hash = line.find('#');
		if (hash != std::string_view::npos) line = line.substr(0, hash);
		line = Trim(line);
		if (line.empty()) continue;
		const size_t sp = line.find(' ');
		const std::string_view word = line.substr(0, sp);
		const std::string_view rest = sp == std::string_view::npos ? std::string_view{} : Trim(line.substr(sp + 1));
		if (word == "topology") {
			b.t.name = std::string(rest);
		} else if (word == "port") {
			b.ParsePort(rest);
		} else if (word == "node") {
			b.ParseNode(rest);
		} else if (word == "prio") {
			for (const std::string &r : SplitList(rest)) prio.push_back(r);
		} else {
			b.error = fmt::format("unknown statement '{}'", word);
		}
	}
	if (b.error.empty()) {
		for (const std::string &r : prio) {
			auto it = b.route_index.find(r);
			if (it == b.route_index.end()) {
				b.error = fmt::format("prio names unknown route {}", r);
				break;
			}
			b.t.routes[it->second].prio = true;
		}
	}
	if (b.error.empty()) b.Derive();
	if (!b.error.empty()) {
		*error = b.error;
		return {};
	}
	return std::move(b.t);
}

std::string PlanarTText(int lanes)
{
	const auto list = [&](const std::string &letters) {
		std::string s;
		for (int k = 1; k <= lanes; k++) s += fmt::format("{}{}{}", s.empty() ? "" : ", ", letters, k);
		return s;
	};
	const auto lanes_of = [&](char a, char b) { return list(std::string(1, a)) + ", " + list(std::string(1, b)); };
	const auto interleaved = [&](char a, char b) {
		std::string s;
		for (int k = 1; k <= lanes; k++) s += fmt::format("{}{}{}, {}{}", s.empty() ? "" : ", ", a, k, b, k);
		return s;
	};
	std::string text = fmt::format("topology T{}\n", lanes);
	text += "# The engineer's T drawing turned half a turn (main line W-E, branch on S): A E->S, B E->W, C S->W, D S->E, E W->E, F W->S.\n";
	text += fmt::format("port W in E: {}\n", list("EF"));
	text += fmt::format("port W out W: {}\n", list("CB"));
	text += fmt::format("port E out E: {}\n", list("ED"));
	text += fmt::format("port E in W: {}\n", list("AB"));
	text += fmt::format("port S in N: {}\n", list("CD"));
	text += fmt::format("port S out S: {}\n", list("AF"));
	text += fmt::format("node {} -> {}\n", list("AB"), lanes_of('A', 'B'));
	text += fmt::format("node {} -> {}\n", list("CD"), lanes_of('C', 'D'));
	text += fmt::format("node {} -> {}\n", list("EF"), lanes_of('E', 'F'));
	text += fmt::format("node {} -> {}\n", interleaved('A', 'F'), list("AF"));
	text += fmt::format("node {} -> {}\n", interleaved('C', 'B'), list("CB"));
	text += fmt::format("node {} -> {}\n", interleaved('E', 'D'), list("ED"));
	text += fmt::format("prio {}, {}\n", list("B"), list("E"));
	return text;
}

uint32_t PlanarLineByName(const PlanarTopology &topology, std::string_view name)
{
	for (uint32_t l = 0; l < topology.lines.size(); l++) {
		if (topology.lines[l].name == name) return l;
	}
	return UINT32_MAX;
}

std::vector<std::string> PlanarCrossingNames(const PlanarTopology &topology)
{
	std::vector<std::string> out;
	for (const PlanarCrossing &c : topology.crossings) {
		std::string a = topology.lines[c.a].name, b = topology.lines[c.b].name;
		if (b < a) std::swap(a, b);
		out.push_back(a + "×" + b);
	}
	std::ranges::sort(out);
	return out;
}
