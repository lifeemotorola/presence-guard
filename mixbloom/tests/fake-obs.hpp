/* Test-only simulated OBS. Copyright (C) 2026 Emmanuel Suah, GPL-2.0-or-later. */
#pragma once
#include <cstdint>
#include <string>

struct FakeSource {
	std::string uuid;
	std::string name;
	uint32_t flags = 0;
	float gain = 1.0f;
	uint32_t tracks = 1;
	bool muted = false;
	bool monitoring = false;
	bool locked = false;
	bool removed = false;
	int refs = 0;
};

void fakeReset();
FakeSource &fakeAdd(const std::string &uuid, const std::string &name, uint32_t flags);
FakeSource *fakeFind(const std::string &uuid);
int fakeWrites();
int fakeAcquired();
int fakeReleased();
int fakeMeters();
void fakeEmitPeak(const std::string &uuid, float leftDb, float rightDb);
