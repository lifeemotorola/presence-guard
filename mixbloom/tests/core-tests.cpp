/* MixBloom model tests. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
#include "mix-core.hpp"
#include <cmath>
#include <cstdio>
#include <limits>

static int checks = 0, failures = 0;
static void check(bool condition, const char *message)
{
	++checks;
	if (!condition) { ++failures; std::printf("FAIL  %s\n", message); }
}

int main()
{
	using namespace mixbloom;
	check(faderToGain(0) == 0.0f, "fader silence");
	check(faderToGain(1000) == 1.0f, "fader unity");
	check(std::fabs(faderToGain(500) - 0.125f) < 0.000001f, "cubic fader");
	check(faderToGain(-99) == 0.0f && faderToGain(9999) == 1.0f, "fader clamps");
	bool roundTrip = true;
	for (int i = 0; i <= 1000; ++i)
		roundTrip &= std::abs(gainToFader(faderToGain(i)) - i) <= 1;
	check(roundTrip, "all 1001 positions round-trip");
	check(gainToFader(4.0f) == 1000, "externally boosted gain is not represented above unity");
	check(gainToFader(std::numeric_limits<float>::quiet_NaN()) == 0, "invalid gain safe");
	check(std::isinf(gainToDb(0)) && gainToDb(0) < 0, "silence is minus infinity");
	check(std::fabs(gainToDb(1)) < 0.001f, "unity is zero dB");
	check(std::fabs(gainToDb(2) - 6.0206f) < 0.001f, "external boost dB is displayed");
	check(peakToPercent(-60) == 0 && peakToPercent(-30) == 50 && peakToPercent(0) == 100,
	      "peak scale");
	check(peakToPercent(-999) == 0 && peakToPercent(6) == 100 &&
	      peakToPercent(std::numeric_limits<float>::quiet_NaN()) == 0, "peak bounds and NaN");
	check(withTrack(0x80000001u, 6, true) == 0x80000021u, "track 6 preserves unrelated bits");
	check(withTrack(0x80000021u, 1, false) == 0x80000020u, "removing track preserves unrelated bits");
	check(withTrack(0x80000021u, 0, true) == 0x80000021u &&
	      withTrack(0x80000021u, 7, false) == 0x80000021u, "invalid track has no effect");
	check(trackEnabled(0x21, 1) && trackEnabled(0x21, 6) && !trackEnabled(0x21, 2) &&
	      !trackEnabled(0x21, 0) && !trackEnabled(0x21, 32), "track lookup and bounds");
	std::printf("%d model checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
