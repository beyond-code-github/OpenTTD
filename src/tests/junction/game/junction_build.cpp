/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file junction_build.cpp Test functionality from junction_build. */

#include "../../../stdafx.h"

#include "../../../3rdparty/catch2/catch.hpp"

#include "../../../junction/game/junction_build.h"
#include "../../../map_func.h"

#include <vector>

#include "../../../safeguards.h"

/** Poster that records posted steps and behaves as configured per call. */
class FakeStepPoster : public JunctionStepPoster {
public:
	/** What a call to Post does. */
	enum class Behaviour { SyncOk, SyncFail, Async, Refuse };

	JunctionSequencer &seq;
	std::vector<TileIndex> posted; ///< Tiles of the posted steps, in order.
	std::vector<Behaviour> script; ///< Behaviour per call; calls beyond the script use SyncOk.

	explicit FakeStepPoster(JunctionSequencer &seq) : seq(seq) {}

	bool Post(const JunctionBuildStep &step) override
	{
		size_t call = this->posted.size();
		this->posted.push_back(std::get<JunctionRailStep>(step).tile);
		Behaviour behaviour = call < this->script.size() ? this->script[call] : Behaviour::SyncOk;
		switch (behaviour) {
			case Behaviour::SyncOk:
				this->seq.OnStepResult(true);
				return true;
			case Behaviour::SyncFail:
				this->seq.OnStepResult(false);
				return false;
			case Behaviour::Async:
				return true;
			case Behaviour::Refuse:
				return false;
		}
		return false;
	}
};

/** @return Five dummy rail steps on distinct tiles. */
static std::vector<JunctionBuildStep> MakeDummySteps()
{
	std::vector<JunctionBuildStep> steps;
	for (uint x = 1; x <= 5; x++) steps.push_back(JunctionRailStep{TileXY(x, 1), Track::X});
	return steps;
}

/**
 * Tick until the state is no longer Building.
 * @param seq The sequencer.
 * @param poster The poster.
 */
static void TickUntilSettled(JunctionSequencer &seq, FakeStepPoster &poster)
{
	for (int i = 0; i < 20 && seq.GetState() == JunctionBuildState::Building; i++) seq.Tick(poster);
}

TEST_CASE("JunctionSequencer outer: builds every step in order")
{
	Map::Allocate(64, 64);
	JunctionSequencer seq;
	FakeStepPoster poster(seq);
	seq.Start(MakeDummySteps());
	TickUntilSettled(seq, poster);

	CHECK(poster.posted == std::vector<TileIndex>{TileXY(1, 1), TileXY(2, 1), TileXY(3, 1), TileXY(4, 1), TileXY(5, 1)});
	CHECK(seq.GetState() == JunctionBuildState::Done);
	CHECK(seq.GetDone() == 5);
	CHECK(seq.GetTotal() == 5);
}

TEST_CASE("JunctionSequencer outer: a failure stops, retry resumes from the failed step")
{
	Map::Allocate(64, 64);
	JunctionSequencer seq;
	FakeStepPoster poster(seq);
	poster.script = {FakeStepPoster::Behaviour::SyncOk, FakeStepPoster::Behaviour::SyncOk, FakeStepPoster::Behaviour::SyncFail};
	seq.Start(MakeDummySteps());
	TickUntilSettled(seq, poster);

	CHECK(seq.GetState() == JunctionBuildState::Failed);
	CHECK(seq.GetDone() == 2);
	CHECK(poster.posted == std::vector<TileIndex>{TileXY(1, 1), TileXY(2, 1), TileXY(3, 1)});

	for (int i = 0; i < 3; i++) seq.Tick(poster);
	CHECK(poster.posted.size() == 3);

	seq.Retry();
	TickUntilSettled(seq, poster);
	CHECK(seq.GetState() == JunctionBuildState::Done);
	CHECK(poster.posted == std::vector<TileIndex>{TileXY(1, 1), TileXY(2, 1), TileXY(3, 1), TileXY(3, 1), TileXY(4, 1), TileXY(5, 1)});
}

TEST_CASE("JunctionSequencer: waits for an asynchronous result")
{
	Map::Allocate(64, 64);
	JunctionSequencer seq;
	FakeStepPoster poster(seq);
	poster.script = {FakeStepPoster::Behaviour::Async};
	seq.Start(MakeDummySteps());

	seq.Tick(poster);
	CHECK(poster.posted.size() == 1);
	seq.Tick(poster);
	CHECK(poster.posted.size() == 1);

	seq.OnStepResult(true);
	seq.Tick(poster);
	REQUIRE(poster.posted.size() == 2);
	CHECK(poster.posted[1] == TileXY(2, 1));
}

TEST_CASE("JunctionSequencer: a refused post counts as a failure")
{
	Map::Allocate(64, 64);
	JunctionSequencer seq;
	FakeStepPoster poster(seq);
	poster.script = {FakeStepPoster::Behaviour::Refuse};
	seq.Start(MakeDummySteps());

	seq.Tick(poster);
	CHECK(seq.GetState() == JunctionBuildState::Failed);
	CHECK(seq.GetDone() == 0);
}

TEST_CASE("JunctionSequencer: cancel returns to idle")
{
	Map::Allocate(64, 64);
	JunctionSequencer seq;
	FakeStepPoster poster(seq);
	seq.Start(MakeDummySteps());
	seq.Cancel();

	CHECK(seq.GetState() == JunctionBuildState::Idle);
	CHECK(seq.GetTotal() == 0);
	seq.Tick(poster);
	CHECK(poster.posted.empty());
}

TEST_CASE("JunctionSequencer: a stray result while idle is ignored")
{
	JunctionSequencer seq;
	seq.OnStepResult(true);
	CHECK(seq.GetState() == JunctionBuildState::Idle);
}
