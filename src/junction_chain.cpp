/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_chain.cpp Junction curve speed as pairwise gap constraints implementation. */

#include "stdafx.h"

#include "junction_chain.h"

#include "direction_func.h"
#include "junction_generator.h"
#include "junction_rules_internal.h"

#include <algorithm>
#include <array>
#include <map>
#include <string_view>

#include "safeguards.h"

std::vector<JunctionCurveGap> JunctionCurveGaps(const std::vector<JunctionCurveHand> &hands, uint8_t tl, uint8_t curve_speed, uint16_t reference_speed)
{
	const int n = 2 * tl;
	const int cl = static_cast<int>(DeriveCurveSpacing(reference_speed, curve_speed, tl));

	std::vector<JunctionCurveGap> gaps;
	for (size_t i = 0; i < hands.size(); i++) {
		for (size_t j = i + 1; j < hands.size(); j++) {
			int gap;
			if (j == i + 1) {
				gap = hands[i] != hands[j] ? 2 : std::min(n - 1, std::max(2, cl));
			} else {
				gap = std::min(n - 1, static_cast<int>(j - i) * cl);
			}
			gaps.push_back({static_cast<uint16_t>(i), static_cast<uint16_t>(j), static_cast<int16_t>(gap)});
		}
	}
	return gaps;
}

bool JunctionCurveGapsHold(const std::vector<JunctionCurveGap> &gaps, const std::vector<int> &pos)
{
	for (const JunctionCurveGap &g : gaps) {
		if (pos[g.j] - pos[g.i] < g.min_gap) return false;
	}
	return true;
}

/** Whether a path is a main line, as CheckPrio reads it. */
static bool IsMainLine(const JunctionPath &path)
{
	return (path.from == JunctionPortRole::EIn && path.to == JunctionPortRole::EOut) || (path.from == JunctionPortRole::WIn && path.to == JunctionPortRole::WOut);
}

std::vector<JunctionChain> ExtractJunctionChains(const JunctionLayout &layout, const JunctionSpec &spec)
{
	const JunctionCheckContext ctx = MakeJunctionCheckContext(layout, spec, FindJunctionPorts(layout), JunctionTMovements());
	const auto used = UsedTracks(ctx);

	std::vector<std::vector<SwitchEvent>> events;
	for (const JunctionPath &path : ctx.paths) events.push_back(SwitchEventsOf(layout, used, path));

	std::vector<JunctionChain> chains;
	for (size_t pi = 0; pi < ctx.paths.size(); pi++) {
		const JunctionPath &path = ctx.paths[pi];
		const std::vector<JunctionPiece> &pieces = path.pieces;
		JunctionChain chain{path.from, path.to, 0, {}, {}};

		/* The unit at which each piece starts, and the curves between pieces. */
		std::vector<uint16_t> start(pieces.size());
		uint16_t unit = 0;
		for (size_t i = 0; i < pieces.size(); i++) {
			start[i] = unit;
			if (i > 0) {
				const DirDiff diff = DirDifference(TrackdirToDirection(pieces[i - 1].trackdir), TrackdirToDirection(pieces[i].trackdir));
				if (diff == DirDiff::Left45) {
					chain.curves.push_back({unit, JunctionCurveHand::Left, pieces[i].tile});
				} else if (diff == DirDiff::Right45) {
					chain.curves.push_back({unit, JunctionCurveHand::Right, pieces[i].tile});
				} else if (diff != DirDiff::Same) {
					chain.steps90++;
				}
			}
			unit += pieces[i].units;
		}
		chain.units = unit;

		for (const SwitchEvent &event : events[pi]) {
			bool prio = false;
			const JunctionPiece &merge_piece = pieces[event.second];
			if (!event.first && IsMainLine(path)) {
				for (size_t qi = 0; qi < ctx.paths.size() && !prio; qi++) {
					if (qi == pi) continue;
					for (const SwitchEvent &other : events[qi]) {
						const JunctionPiece &piece = ctx.paths[qi].pieces[other.second];
						if (other.first || piece.tile.x != merge_piece.tile.x || piece.tile.y != merge_piece.tile.y || piece.trackdir == merge_piece.trackdir) continue;
						/* A lane merge has the same ends and no prio. */
						prio = ctx.paths[qi].from != path.from || ctx.paths[qi].to != path.to;
						break;
					}
				}
			}
			chain.switches.push_back({start[event.second], event.first, prio, merge_piece.tile});
		}
		chains.push_back(std::move(chain));
	}
	return chains;
}

std::vector<std::string> JunctionChainViolations(const std::vector<JunctionChain> &chains, const JunctionSpec &spec)
{
	std::vector<std::string> out;
	const int cover = 2 * static_cast<int>(spec.prio_tiles);
	for (size_t ci = 0; ci < chains.size(); ci++) {
		const JunctionChain &chain = chains[ci];
		if (chain.steps90 > 0) out.push_back(fmt::format("chain {}: {} 90-degree step(s)", ci, chain.steps90));

		std::vector<JunctionCurveHand> hands;
		std::vector<int> pos;
		for (const JunctionChainCurve &curve : chain.curves) {
			hands.push_back(curve.hand);
			pos.push_back(curve.unit);
		}
		for (const JunctionCurveGap &gap : JunctionCurveGaps(hands, spec.tl, spec.curve_speed, spec.reference_speed)) {
			const int actual = pos[gap.j] - pos[gap.i];
			if (actual < gap.min_gap) out.push_back(fmt::format("chain {} curves {},{} gap {} below {}", ci, gap.i, gap.j, actual, gap.min_gap));
		}

		for (const JunctionChainSwitch &merge : chain.switches) {
			if (!merge.prio) continue;
			const int m = merge.unit;
			for (const JunctionChainCurve &curve : chain.curves) {
				if (curve.unit > m - cover && curve.unit < m) out.push_back(fmt::format("chain {} merge at unit {}: curve at unit {} inside the prio coverage", ci, m, curve.unit));
			}
			for (const JunctionChainSwitch &split : chain.switches) {
				if (split.split && split.unit >= m - cover && split.unit < m) out.push_back(fmt::format("chain {} merge at unit {}: split at unit {} inside the prio coverage", ci, m, split.unit));
			}
		}
	}
	return out;
}

std::string FormatJunctionChain(const JunctionChain &chain)
{
	static constexpr std::array<std::string_view, 6> names = {"EIn", "EOut", "WIn", "WOut", "NIn", "SOut"};
	struct Item { uint16_t unit; std::string text; };
	std::vector<Item> items;
	for (const JunctionChainCurve &c : chain.curves) {
		items.push_back({c.unit, fmt::format("C{}{}@{},{}", c.unit, c.hand == JunctionCurveHand::Left ? 'L' : 'R', c.tile.x, c.tile.y)});
	}
	for (const JunctionChainSwitch &s : chain.switches) {
		items.push_back({s.unit, s.split ? fmt::format("S{}@{},{}", s.unit, s.tile.x, s.tile.y) : fmt::format("M{}{}@{},{}", s.unit, s.prio ? "p" : "", s.tile.x, s.tile.y)});
	}
	std::ranges::stable_sort(items, [](const Item &a, const Item &b) { return a.unit < b.unit; });
	std::string line = fmt::format("{}->{} units {}:", names[to_underlying(chain.from)], names[to_underlying(chain.to)], chain.units);
	for (const Item &item : items) line += " " + item.text;
	return line;
}
