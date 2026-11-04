/*
Hype Clip for OBS Studio
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
 * "Should we clip that?" is pure logic, so it is tested here without OBS, Qt
 * or a microphone: the harness feeds synthetic meters and a fake clock.
 * Build with tests/run-tests.sh.
 */

#include "hype-core.hpp"

#include <cstdio>
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

using namespace hype;

/* A detector with fast settings, so the tests run in milliseconds of fake
 * time instead of tens of seconds. */
static Config testConfig()
{
	Config config;
	config.enabled = true;
	config.autoClip = true;
	config.thresholdDb = -12;
	config.marginDb = 6;
	config.sustainMs = 500;
	config.releaseMs = 200;
	config.cooldownMs = 3000;
	config.baselineMs = 1000;
	return config;
}

static std::vector<Level> loud(float db)
{
	return {Level{db, true}};
}

static std::vector<Level> dead()
{
	return {Level{-100.0f, false}};
}

/*
 * Runs the detector for `fromMs` until `toMs` in 100 ms steps, feeding `levels`
 * each time, and counts how many clips it asks for.
 */
static int runFor(Detector &detector, int64_t fromMs, int64_t toMs, const std::vector<Level> &levels,
		  Request *last = nullptr)
{
	int wanted = 0;

	for (int64_t now = fromMs; now <= toMs; now += 100) {
		detector.submit(now, levels);
		const Request request = detector.tick(now);
		if (request.wanted) {
			wanted++;
			if (last)
				*last = request;
		}
	}

	return wanted;
}

int main()
{
	std::printf("a quiet room\n");
	{
		Detector detector;
		detector.setConfig(testConfig());

		check(runFor(detector, 100, 6000, loud(-40.0f)) == 0, "normal speech is not a moment");
		check(runFor(detector, 6100, 8000, dead()) == 0, "a meter that reports nothing is never a moment");
	}

	std::printf("a shout\n");
	{
		Detector detector;
		detector.setConfig(testConfig());

		check(runFor(detector, 100, 1000, loud(-40.0f)) == 0, "the room is learned first");

		Request request;
		const int wanted = runFor(detector, 1100, 2000, loud(-5.0f), &request);
		check(wanted == 1, "a sustained shout asks for exactly one clip");
		check(request.wanted, "the request is marked wanted");
		check(request.scoreDb >= -5.0f, "the score is the loudest sample of the streak");
		check(request.sinceMs >= 1100 && request.sinceMs <= 1200,
		      "the request remembers when the streak started");

		check(runFor(detector, 2100, 4000, loud(-5.0f)) == 0, "the same streak does not ask twice");
	}

	std::printf("how loud counts\n");
	{
		Detector detector;
		detector.setConfig(testConfig());
		(void)runFor(detector, 100, 1000, loud(-40.0f));

		/* Loud, but not loud enough in absolute terms. */
		check(runFor(detector, 1100, 3000, loud(-20.0f)) == 0, "a level under the threshold is not hype");

		/* Loud enough, but the room has been that loud all along. */
		Detector steady;
		steady.setConfig(testConfig());
		(void)runFor(steady, 100, 4000, loud(-10.0f));
		check(runFor(steady, 4100, 9000, loud(-10.0f)) == 0, "a room that is always loud does not clip itself");
	}

	std::printf("the baseline adapts\n");
	{
		Config config = testConfig();
		config.cooldownMs = 0;
		config.baselineMs = 500;

		Detector detector;
		detector.setConfig(config);

		int wanted = 0;
		for (int64_t now = 100; now <= 12000; now += 100) {
			detector.submit(now, loud(-5.0f));
			if (detector.tick(now).wanted)
				wanted++;
		}

		check(wanted == 1, "constant loud audio stops clipping once the baseline has caught up");
		check(detector.status(12000).baselineDb > -12.0f, "the baseline follows the room upwards");

		/* Back to a quiet room: the baseline comes back down again. */
		(void)runFor(detector, 12100, 24000, loud(-45.0f));
		check(detector.status(24000).baselineDb < -30.0f, "the baseline follows the room back down");
	}

	std::printf("drop-outs\n");
	{
		Detector detector;
		detector.setConfig(testConfig());
		(void)runFor(detector, 100, 1000, loud(-40.0f));

		/* A single quiet sample inside the shout is just a breath. */
		(void)runFor(detector, 1100, 1300, loud(-5.0f));
		(void)runFor(detector, 1400, 1400, loud(-40.0f));
		check(runFor(detector, 1500, 2200, loud(-5.0f)) == 1, "a short drop-out does not break the streak");

		/* A real pause ends it, and the next shout has to build up again. */
		Detector second;
		second.setConfig(testConfig());
		(void)runFor(second, 100, 1000, loud(-40.0f));
		(void)runFor(second, 1100, 1300, loud(-5.0f));
		(void)runFor(second, 1400, 2000, loud(-40.0f));
		check(runFor(second, 2100, 2400, loud(-5.0f)) == 0, "a real pause ends the streak");
		check(runFor(second, 2500, 3200, loud(-5.0f)) == 1, "the next streak is counted from its own start");
	}

	std::printf("the cooldown\n");
	{
		Detector detector;
		detector.setConfig(testConfig());
		(void)runFor(detector, 100, 1000, loud(-40.0f));

		Request request;
		(void)runFor(detector, 1100, 1800, loud(-5.0f), &request);

		/* The clip that request produced has been saved. */
		detector.noteClip(1800, -5.0f);
		check(detector.status(1800).cooldownLeftMs == 3000, "saving a clip starts the cooldown");

		(void)runFor(detector, 1900, 2400, loud(-40.0f));
		check(runFor(detector, 2500, 4000, loud(-5.0f)) == 0, "nothing is clipped during the cooldown");
		check(detector.status(4000).cooldownLeftMs > 0, "the cooldown is still running");

		/* One continuous moment is clipped once; a new moment after the
		 * cooldown is clipped again. */
		(void)runFor(detector, 4100, 4800, loud(-40.0f));
		check(runFor(detector, 4900, 6000, loud(-5.0f)) == 1, "clipping works again once the cooldown is over");
	}

	std::printf("a manual clip\n");
	{
		Detector detector;
		detector.setConfig(testConfig());
		(void)runFor(detector, 100, 1000, loud(-40.0f));
		(void)runFor(detector, 1100, 1500, loud(-5.0f));

		/* The operator pressed Clip that in the middle of the shout. */
		detector.noteClip(1550, -5.0f);
		check(runFor(detector, 1600, 3000, loud(-5.0f)) == 0,
		      "a manual clip is not followed by an automatic one");
	}

	std::printf("switches\n");
	{
		Detector disabled;
		Config config = testConfig();
		config.enabled = false;
		disabled.setConfig(config);
		(void)runFor(disabled, 100, 1000, loud(-40.0f));
		check(runFor(disabled, 1100, 3000, loud(-5.0f)) == 0, "a disabled detector never asks for a clip");

		Detector manualOnly;
		Config manualConfig = testConfig();
		manualConfig.autoClip = false;
		manualOnly.setConfig(manualConfig);
		(void)runFor(manualOnly, 100, 1000, loud(-40.0f));
		check(runFor(manualOnly, 1100, 1600, loud(-5.0f)) == 0, "automatic clipping can be switched off");
		check(manualOnly.status(1600).loudNow, "the meter still shows the moment");
		check(runFor(manualOnly, 1700, 3000, loud(-5.0f)) == 0, "and still does not clip");
	}

	std::printf("several meters\n");
	{
		Detector detector;
		detector.setConfig(testConfig());
		(void)runFor(detector, 100, 1000, {Level{-40.0f, true}, Level{-45.0f, true}});

		/* Only the second microphone is screaming. */
		check(runFor(detector, 1100, 2000, {Level{-45.0f, true}, Level{-4.0f, true}}) == 1,
		      "the loudest live meter decides");

		/* One dead, one quiet: nothing to clip. */
		Detector halfDead;
		halfDead.setConfig(testConfig());
		(void)runFor(halfDead, 100, 1000, {Level{-40.0f, true}, Level{-100.0f, false}});
		check(runFor(halfDead, 1100, 3000, {Level{-20.0f, true}, Level{-100.0f, false}}) == 0,
		      "a dead second microphone does not change the answer");

		Detector allDead;
		allDead.setConfig(testConfig());
		check(runFor(allDead, 100, 5000, dead()) == 0, "nothing is clipped when no meter is alive");
		check(!allDead.status(5000).anyMeterAlive, "the status reports that no meter is alive");
	}

	std::printf("status\n");
	{
		Detector detector;
		detector.setConfig(testConfig());
		(void)runFor(detector, 100, 1000, loud(-40.0f));

		Status quiet = detector.status(1000);
		check(!quiet.loudNow, "a quiet room is not loud");
		check(quiet.loudForMs == 0, "no streak means no streak length");
		check(quiet.anyMeterAlive, "the meter is alive");

		(void)runFor(detector, 1100, 1500, loud(-5.0f));
		Status loudStatus = detector.status(1500);
		check(loudStatus.loudNow, "the shout is loud now");
		check(loudStatus.loudForMs >= 400, "the streak length grows while it lasts");
		check(loudStatus.peakDb >= -5.0f, "the peak of the streak is reported");
	}

	std::printf("config clamping\n");
	{
		Config wild;
		wild.thresholdDb = 40;
		wild.marginDb = 99;
		wild.sustainMs = 1;
		wild.releaseMs = -20;
		wild.cooldownMs = -1000;
		wild.baselineMs = 5;

		Detector detector;
		detector.setConfig(wild);
		const Config fixed = detector.config();

		check(fixed.thresholdDb == 0, "the threshold is clamped to the dB range");
		check(fixed.marginDb == 30, "the margin is capped");
		check(fixed.sustainMs == 100, "the sustain has a floor");
		check(fixed.releaseMs == 0, "a negative release is clamped to zero");
		check(fixed.cooldownMs == 0, "a negative cooldown is clamped to zero");
		check(fixed.baselineMs == 1000, "the baseline window has a floor");

		Config low;
		low.thresholdDb = -200;
		detector.setConfig(low);
		check(detector.config().thresholdDb == -60, "the threshold has a floor");
	}

	std::printf("reset\n");
	{
		Detector detector;
		detector.setConfig(testConfig());
		(void)runFor(detector, 100, 1000, loud(-40.0f));
		(void)runFor(detector, 1100, 1600, loud(-5.0f));
		detector.noteClip(1600, -5.0f);

		detector.reset(1700);
		const Status status = detector.status(1700);
		check(!status.loudNow, "a reset forgets the streak");
		check(status.cooldownLeftMs == 0, "a reset forgets the cooldown");
		check(status.baselineDb == Detector::kBaselineStartDb, "a reset forgets the baseline");

		(void)runFor(detector, 1800, 2200, loud(-40.0f));
		check(runFor(detector, 2300, 3200, loud(-5.0f)) == 1, "the detector works again after a reset");
	}

	std::printf("\n%s (%d checks, %d failure%s)\n", g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", g_checks,
		    g_failures, g_failures == 1 ? "" : "s");

	return g_failures == 0 ? 0 : 1;
}
