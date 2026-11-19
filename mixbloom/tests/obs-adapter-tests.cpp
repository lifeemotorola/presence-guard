/*
Real OBS adapter exercised against a fake OBS implementation.
Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later; no warranty.
*/
#include "obs-mixer.hpp"
#include "fake-obs.hpp"
#include <obs-module.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <memory>
#include <thread>

static int checks = 0, failures = 0;
static void check(bool condition, const char *message)
{
	++checks;
	if (!condition) {
		++failures;
		std::printf("FAIL  %s\n", message);
	}
}

int main()
{
	using namespace mixbloom;
	fakeReset();
	auto &mic = fakeAdd("mic-id", "Same name", OBS_SOURCE_AUDIO);
	auto &second = fakeAdd("second-id", "Same name", OBS_SOURCE_AUDIO);
	auto &desktop = fakeAdd("desktop-id", "Desktop Audio", OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_SELF_MONITOR);
	auto &camera = fakeAdd("camera-id", "Video only", OBS_SOURCE_VIDEO);
	auto &gone = fakeAdd("gone-id", "Removed", OBS_SOURCE_AUDIO);
	auto &internal = fakeAdd("internal-id", "Internal submix", OBS_SOURCE_AUDIO | OBS_SOURCE_SUBMIX);
	fakeAdd("", "Missing UUID", OBS_SOURCE_AUDIO);
	(void)camera;
	(void)internal;
	gone.removed = true;
	second.locked = true;
	mic.tracks = 0x80000001u;
	desktop.monitoring = true;

	const auto snapshot = readSources();
	check(snapshot.size() == 3, "only public usable audio inputs are listed");
	check(snapshot[0].uuid == "desktop-id" && snapshot[1].uuid == "mic-id" && snapshot[2].uuid == "second-id",
	      "duplicate names are distinct and sorted by stable UUID");
	check(snapshot[0].monitoring && !snapshot[0].canMonitor && snapshot[2].volumeLocked,
	      "monitoring capability and fader lock are reflected");
	check(fakeWrites() == 0, "enumeration does not change OBS audio");

	check(setFader("mic-id", 500) == Result::Applied && std::fabs(mic.gain - 0.125f) < 0.000001f,
	      "fader writes the selected UUID");
	check(second.gain == 1.0f, "another source with the same name is untouched");
	const int beforeLocked = fakeWrites();
	check(setFader("second-id", 100) == Result::Locked && fakeWrites() == beforeLocked,
	      "honor OBS fader lock even when it changes after refresh");
	check(setFader("mic-id", -1) == Result::Invalid && setFader("mic-id", 1001) == Result::Invalid,
	      "reject out-of-range faders");
	check(setFader("missing", 500) == Result::Missing && setFader("gone-id", 500) == Result::Missing &&
		      setFader("camera-id", 500) == Result::Missing,
	      "missing, removed and non-audio sources cannot be changed");
	check(setMuted("mic-id", true) == Result::Applied && mic.muted && !second.muted,
	      "mute changes only the targeted source");
	check(setMuted("mic-id", false) == Result::Applied && !mic.muted, "unmute explicitly");

	check(setTrack("mic-id", 1, false) == Result::ConfirmClearTracks && mic.tracks == 0x80000001u,
	      "removing the last output track needs confirmation");
	check(setTrack("mic-id", 1, false, true) == Result::Applied && mic.tracks == 0x80000000u,
	      "confirmed clear preserves unrelated mask bits");
	check(setTrack("mic-id", 6, true) == Result::Applied && mic.tracks == 0x80000020u, "track six can be enabled");
	mic.tracks |= 2u; // simulate an external OBS change after the last UI refresh
	check(setTrack("mic-id", 3, true) == Result::Applied && mic.tracks == 0x80000026u,
	      "routing reads the current mask and preserves external changes");
	const int beforeInvalid = fakeWrites();
	check(setTrack("mic-id", 0, true) == Result::Invalid && setTrack("mic-id", 7, false) == Result::Invalid &&
		      fakeWrites() == beforeInvalid,
	      "invalid track numbers do not write");
	check(setMonitoring("desktop-id", true) == Result::Unsupported && desktop.monitoring,
	      "cannot enable risky desktop self-monitoring");
	check(setMonitoring("desktop-id", false) == Result::Applied && !desktop.monitoring,
	      "can disable existing risky monitoring");
	check(setMonitoring("mic-id", true) == Result::Applied && mic.monitoring &&
		      setMonitoring("mic-id", false) == Result::Applied && !mic.monitoring,
	      "mic monitoring round trip");

	{
		MeterTap meter("mic-id"), missing("missing");
		check(meter.attached() && !missing.attached() && fakeMeters() == 1,
		      "meter attaches only to an existing source");
		check(meter.sample().sequence == 0, "meter starts with no samples");
		std::thread audio([] { fakeEmitPeak("mic-id", -20.0f, -7.5f); });
		audio.join();
		const PeakSample sample = meter.sample();
		check(sample.sequence == 1 && std::fabs(sample.db - (-7.5f)) < 0.001f,
		      "audio-thread callback publishes the highest channel through atomics");
	}
	check(fakeMeters() == 0, "meter callback is removed and meter destroyed");
	{
		auto meter = std::make_unique<MeterTap>("mic-id");
		std::atomic<bool> running{true}, started{false};
		std::thread audio([&] {
			started.store(true);
			while (running.load())
				fakeEmitPeak("mic-id", -20.0f, -10.0f);
		});
		while (!started.load())
			std::this_thread::yield();
		meter.reset();
		running.store(false);
		audio.join();
		check(fakeMeters() == 0, "meter teardown is safe while simulated audio callbacks run");
	}
	{
		SourceIdentity identity("mic-id");
		check(identity.stillCurrent(), "confirmation identity initially matches the source");
		mic.removed = true;
		auto &replacement = fakeAdd("mic-id", "Replacement", OBS_SOURCE_AUDIO);
		check(!identity.stillCurrent(), "confirmation identity rejects a replacement using the same UUID");
		check(replacement.gain == 1.0f, "replacement remains untouched");
	}
	check(fakeAcquired() == fakeReleased() && mic.refs == 0 && second.refs == 0 && gone.refs == 0,
	      "all strong OBS references are released");
	std::printf("%d fake-OBS adapter checks, %d failures\n", checks, failures);
	fakeReset();
	return failures ? 1 : 0;
}
