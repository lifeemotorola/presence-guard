/*
Auto Director for OBS Studio
Copyright (C) 2026 Presence Guard contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/*
 * The directing policy is pure logic, so it is tested here without OBS, Qt or
 * a running stream: the harness feeds it levels and clock readings and checks
 * the scene it would cut to. Build with tests/run-tests.sh.
 */

#include "director-core.hpp"

#include <cstdio>
#include <string>
#include <vector>

static int g_failures = 0;
static int g_checks = 0;

static void check(bool condition, const char *what)
{
	g_checks++;
	if (condition) {
		std::printf("  PASS  %s\n", what);
		return;
	}

	g_failures++;
	std::printf("  FAIL  %s\n", what);
}

using namespace director;

static Level loud(float db)
{
	return Level{db, true};
}

static Level silent()
{
	return Level{-70.0f, true};
}

static Level dead()
{
	return Level{-100.0f, false};
}

static std::vector<Take> threeTakes()
{
	return {
		Take{"Cam A", "Mic A", true},
		Take{"Cam B", "Mic B", true},
		Take{"Cam C", "Mic C", true},
	};
}

int main()
{
	std::printf("starting policy\n");
	{
		Core core;
		Config config;
		config.enabled = true;
		core.setConfig(config);
		core.setTakes(threeTakes());

		/* Nobody has spoken yet: stay put even though a take is loud. */
		core.submitLevels(0, {loud(-20.0f), silent(), silent()});
		Decision d = core.tick(0, "Cam A", true, false);
		check(d.action == Action::None, "nothing happens on the very first tick");
	}

	std::printf("speaker detection\n");
	{
		Core core;
		Config config;
		config.enabled = true;
		config.dwellMs = 1000;
		core.setConfig(config);
		core.setTakes(threeTakes());

		core.tick(0, "Cam A", true, false);
		core.submitLevels(1000, {silent(), loud(-20.0f), silent()});
		core.tick(1000, "Cam A", true, false);
		core.submitLevels(2000, {silent(), loud(-20.0f), silent()});
		Decision d = core.tick(2000, "Cam A", true, false);
		check(d.action == Action::Switch && d.scene == "Cam B", "cuts to the take that speaks");
		check(d.reason.find("Mic B") != std::string::npos, "reason names the microphone");

		/* A take below the threshold must never win. */
		Core quiet;
		Config quiet_config;
		quiet_config.enabled = true;
		quiet_config.thresholdDb = -30;
		quiet.setConfig(quiet_config);
		quiet.setTakes(threeTakes());
		quiet.tick(0, "Cam A", true, false);
		quiet.submitLevels(1000, {silent(), loud(-40.0f), silent()});
		quiet.tick(1000, "Cam A", true, false);
		quiet.submitLevels(5000, {silent(), loud(-40.0f), silent()});
		d = quiet.tick(5000, "Cam A", true, false);
		check(d.action == Action::None, "voice below the threshold is ignored");
	}

	std::printf("dwell time (a new voice has to prove itself)\n");
	{
		Core core;
		Config config;
		config.enabled = true;
		config.dwellMs = 4000;
		core.setConfig(config);
		core.setTakes(threeTakes());

		core.tick(0, "Cam A", true, false);

		/* One noisy sample is a cough, not a speaker. */
		core.submitLevels(1000, {silent(), loud(-20.0f), silent()});
		check(core.tick(1000, "Cam A", true, false).action == Action::None,
		      "a single loud sample does not steal the shot");

		core.submitLevels(4000, {silent(), loud(-20.0f), silent()});
		check(core.tick(4000, "Cam A", true, false).action == Action::None,
		      "still waiting while the dwell is running");

		core.submitLevels(5000, {silent(), loud(-20.0f), silent()});
		Decision d = core.tick(5000, "Cam A", true, false);
		check(d.action == Action::Switch && d.scene == "Cam B",
		      "cuts once the speaker has been talking for the dwell");

		/* Cam C takes over; its clock starts at 5500. */
		core.submitLevels(5500, {silent(), silent(), loud(-10.0f)});
		check(core.tick(5500, "Cam B", true, false).action == Action::None, "no instant re-cut");

		core.submitLevels(9400, {silent(), silent(), loud(-10.0f)});
		check(core.tick(9400, "Cam B", true, false).action == Action::None,
		      "dwell time blocks an early re-cut");

		core.submitLevels(9500, {silent(), silent(), loud(-10.0f)});
		d = core.tick(9500, "Cam B", true, false);
		check(d.action == Action::Switch && d.scene == "Cam C", "cuts once the dwell expires");

		/* A speaker who stops and starts again begins a new dwell. */
		core.submitLevels(10000, {loud(-10.0f), silent(), silent()});
		check(core.tick(10000, "Cam C", true, false).action == Action::None, "interrupted speech starts over");

		core.submitLevels(11000, {silent(), silent(), loud(-10.0f)});
		check(core.tick(11000, "Cam C", true, false).action == Action::None,
		      "the previous contender lost its dwell when it went quiet");
	}

	std::printf("hysteresis\n");
	{
		Core core;
		Config config;
		config.enabled = true;
		config.dwellMs = 0;
		config.hysteresisDb = 6;
		core.setConfig(config);
		core.setTakes(threeTakes());

		core.tick(0, "Cam A", true, false);
		core.submitLevels(100, {loud(-30.0f), silent(), silent()});
		core.tick(100, "Cam A", true, false);
		check(core.tick(100, "Cam A", true, false).action == Action::None,
		      "current take stays while it speaks");

		/* Cam B is only 2 dB louder: not enough to steal the shot. */
		core.submitLevels(200, {loud(-30.0f), loud(-28.0f), silent()});
		Decision d = core.tick(200, "Cam A", true, false);
		check(d.action == Action::None, "a marginal level difference does not steal the shot");

		/* 8 dB louder clears the 6 dB hysteresis. */
		core.submitLevels(300, {loud(-30.0f), loud(-22.0f), silent()});
		d = core.tick(300, "Cam A", true, false);
		check(d.action == Action::Switch && d.scene == "Cam B", "a clear level advantage does steal it");

		/* The take on air going quiet is an immediate excuse to leave. */
		core.submitLevels(400, {silent(), dead(), loud(-40.0f)});
		d = core.tick(400, "Cam A", true, false);
		check(d.action == Action::Switch && d.scene == "Cam C",
		      "an unrelated speaker wins once the current take stops speaking");
	}

	std::printf("lobby fallback\n");
	{
		Core core;
		Config config;
		config.enabled = true;
		config.dwellMs = 0;
		config.fallbackDelaySec = 30;
		config.fallbackScene = "Lobby";
		core.setConfig(config);
		core.setTakes(threeTakes());

		core.tick(0, "Cam A", true, false);
		core.submitLevels(1000, {silent(), silent(), silent()});
		Decision d = core.tick(1000, "Cam A", true, false);
		check(d.action == Action::None, "silence does not fall back before the delay");

		core.submitLevels(31000, {silent(), silent(), silent()});
		d = core.tick(31000, "Cam A", true, false);
		check(d.action == Action::Switch && d.scene == "Lobby",
		      "falls back to the lobby scene after the delay");

		d = core.tick(32000, "Lobby", true, false);
		check(d.action == Action::None, "does not re-issue the fallback every tick");

		/* Somebody comes back: leave the lobby for them. */
		core.submitLevels(33000, {loud(-25.0f), silent(), silent()});
		d = core.tick(33000, "Lobby", true, false);
		check(d.action == Action::Switch && d.scene == "Cam A", "returns from the lobby when someone speaks");

		/* Speech keeps the lobby away. */
		Core chatty;
		Config chatty_config = config;
		chatty.setConfig(chatty_config);
		chatty.setTakes(threeTakes());
		chatty.tick(0, "Cam A", true, false);
		bool left_the_take = false;
		for (int64_t t = 1000; t <= 60000; t += 1000) {
			chatty.submitLevels(t, {loud(-25.0f), silent(), silent()});
			left_the_take = left_the_take || chatty.tick(t, "Cam A", true, false).action != Action::None;
		}
		check(!left_the_take, "a take that keeps talking never triggers the lobby");
	}

	std::printf("dead meters\n");
	{
		Core core;
		Config config;
		config.enabled = true;
		config.dwellMs = 0;
		config.fallbackDelaySec = 5;
		config.fallbackScene = "Lobby";
		core.setConfig(config);
		core.setTakes(threeTakes());

		core.tick(0, "Cam A", true, false);
		core.submitLevels(10000, {dead(), dead(), dead()});
		Decision d = core.tick(10000, "Cam A", true, false);
		check(d.action == Action::None, "no meter data never counts as silence (muted or unplugged mic)");

		core.submitLevels(11000, {silent(), dead(), dead()});
		d = core.tick(11000, "Cam A", true, false);
		check(d.action == Action::Switch && d.scene == "Lobby",
		      "one live meter reporting silence is enough to fall back");
	}

	std::printf("gates\n");
	{
		Core core;
		Config config;
		config.enabled = false;
		config.dwellMs = 0;
		core.setConfig(config);
		core.setTakes(threeTakes());
		core.tick(0, "Cam A", true, false);
		core.submitLevels(100, {silent(), loud(-20.0f), silent()});
		check(core.tick(100, "Cam A", true, false).action == Action::None, "disabled director does nothing");

		Config live_only = config;
		live_only.enabled = true;
		live_only.onlyWhileLive = true;
		core.setConfig(live_only);
		core.tick(200, "Cam A", false, false);
		core.submitLevels(300, {silent(), loud(-20.0f), silent()});
		check(core.tick(300, "Cam A", false, false).action == Action::None,
		      "only-while-live stays idle when offline");

		core.tick(400, "Cam A", true, false);
		core.submitLevels(500, {silent(), loud(-20.0f), silent()});
		check(core.tick(500, "Cam A", true, false).action == Action::Switch,
		      "starts directing once the stream is live");

		Core studio;
		Config studio_config = live_only;
		studio_config.dwellMs = 0;
		studio.setConfig(studio_config);
		studio.setTakes(threeTakes());
		studio.tick(0, "Cam A", true, true);
		studio.submitLevels(1000, {silent(), loud(-20.0f), silent()});
		check(studio.tick(1000, "Cam A", true, true).action == Action::None,
		      "studio mode is left entirely alone");

		Core disabled_take;
		disabled_take.setConfig(studio_config);
		disabled_take.setTakes({Take{"Cam A", "Mic A", true}, Take{"Cam B", "Mic B", false}});
		disabled_take.tick(0, "Cam A", true, false);
		disabled_take.submitLevels(1000, {silent(), loud(-10.0f)});
		check(disabled_take.tick(1000, "Cam A", true, false).action == Action::None,
		      "a disabled take is never cut to");
	}

	std::printf("operator control\n");
	{
		Core core;
		Config config;
		config.enabled = true;
		config.dwellMs = 0;
		config.operatorLockMs = 15000;
		core.setConfig(config);
		core.setTakes(threeTakes());

		core.tick(0, "Cam A", true, false);
		core.noteOperatorChange(100);
		check(core.locked(200), "a manual scene change locks the director");
		core.submitLevels(300, {silent(), loud(-20.0f), silent()});
		check(core.tick(300, "Cam A", true, false).action == Action::None,
		      "no cuts while the operator is in charge");

		core.submitLevels(16000, {silent(), loud(-20.0f), silent()});
		Decision d = core.tick(16000, "Cam A", true, false);
		check(d.action == Action::Switch && d.scene == "Cam B", "directing resumes when the lock expires");

		core.lock(17000);
		core.forceNext();
		check(!core.locked(17000), "asking for the next take releases the lock");
		core.submitLevels(17100, {silent(), silent(), silent()});
		d = core.tick(17100, "Cam B", true, false);
		check(d.action == Action::Switch && d.scene == "Cam C", "next take advances even in silence");
		check(d.reason.find("manual") != std::string::npos, "manual advance is labelled as such");
	}

	std::printf("rebuilding the take list\n");
	{
		Core core;
		Config config;
		config.enabled = true;
		config.dwellMs = 0;
		config.fallbackDelaySec = 5;
		config.fallbackScene = "Lobby";
		core.setConfig(config);
		core.setTakes(threeTakes());

		core.tick(0, "Cam A", true, false);
		core.submitLevels(2000, {silent(), silent(), silent()});
		core.tick(2000, "Cam A", true, false);

		/* The dock rebuilt the meters (a scene was added, the operator hit
		 * "Add all scenes"). No stale silence may be carried over. */
		core.setTakes(threeTakes());
		core.submitLevels(60000, {silent(), silent(), silent()});
		check(core.tick(60000, "Cam A", true, false).action == Action::None,
		      "a rebuilt take list restarts the silence clock");
	}

	std::printf("edge cases\n");
	{
		Core core;
		Config config;
		config.enabled = true;
		config.dwellMs = 0;
		core.setConfig(config);
		core.setTakes(threeTakes());

		core.tick(0, "Cam A", true, false);
		/* Ties go to the take listed first. */
		core.submitLevels(1000, {silent(), loud(-20.0f), loud(-20.0f)});
		Decision d = core.tick(1000, "Cam A", true, false);
		check(d.action == Action::Switch && d.scene == "Cam B", "equal levels keep the operator's order");

		/* An unknown live scene (operator picked a scene that is not a take). */
		core.submitLevels(2000, {silent(), loud(-20.0f), silent()});
		d = core.tick(2000, "Some Other Scene", true, false);
		check(d.action == Action::Switch && d.scene == "Cam B",
		      "an untracked live scene is treated as nobody being on a take");

		/* Fewer levels than takes must not read out of bounds. */
		core.submitLevels(3000, {loud(-20.0f)});
		d = core.tick(3000, "Cam B", true, false);
		check(d.action == Action::Switch && d.scene == "Cam A",
		      "missing level samples are treated as no audio");

		/* Empty take list. */
		Core empty;
		empty.setConfig(config);
		empty.tick(0, "Cam A", true, false);
		check(empty.tick(10, "Cam A", true, false).action == Action::None, "no takes means no directing");
	}

	std::printf("\n%s (%d checks, %d failure%s)\n", g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", g_checks,
		    g_failures, g_failures == 1 ? "" : "s");

	return g_failures == 0 ? 0 : 1;
}
