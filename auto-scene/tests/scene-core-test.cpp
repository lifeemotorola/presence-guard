/*
Auto Scene Adjust for OBS Studio
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
 * The geometry behind Auto Scene Adjust, checked without OBS, Qt or a canvas:
 * every case here is a rectangle somebody got wrong on stream.
 *
 * Build with tests/run-tests.sh.
 */

#include "scene-core.hpp"

#include <cmath>
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

using namespace autoscene;

static bool sameRect(const Rect &a, const Rect &b)
{
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static void checkRect(const Rect &got, const Rect &want, const char *what)
{
	g_checks++;
	if (sameRect(got, want)) {
		std::printf("  PASS  %s\n", what);
		return;
	}

	g_failures++;
	std::printf("  FAIL  %s (got %d,%d %dx%d, wanted %d,%d %dx%d)\n", what, got.x, got.y, got.w, got.h, want.x,
		    want.y, want.w, want.h);
}

static bool overlaps(const Rect &a, const Rect &b)
{
	const int left = std::max(a.x, b.x);
	const int top = std::max(a.y, b.y);
	const int right = std::min(a.x + a.w, b.x + b.w);
	const int bottom = std::min(a.y + a.h, b.y + b.h);
	return right > left && bottom > top;
}

static bool inside(const Rect &inner, int canvasW, int canvasH)
{
	return inner.x >= 0 && inner.y >= 0 && inner.x + inner.w <= canvasW && inner.y + inner.h <= canvasH;
}

static bool noOverlaps(const std::vector<Rect> &cells)
{
	for (size_t i = 0; i < cells.size(); i++)
		for (size_t j = i + 1; j < cells.size(); j++)
			if (overlaps(cells[i], cells[j]))
				return false;
	return true;
}

static bool allInside(const std::vector<Rect> &cells, int canvasW, int canvasH)
{
	for (const Rect &cell : cells)
		if (!inside(cell, canvasW, canvasH))
			return false;
	return true;
}

static Item fitted(const Placement &placement, int srcW, int srcH)
{
	Item item;
	item.srcW = srcW;
	item.srcH = srcH;
	item.posX = placement.posX;
	item.posY = placement.posY;
	item.scaleX = placement.scaleX;
	item.scaleY = placement.scaleY;
	return item;
}

int main()
{
	std::printf("names\n");
	{
		const std::string fit = fitModeName(FitMode::Fit);
		check(fit == "fit", "fit is called fit");
		check(fitModeFromName("fill") == FitMode::Fill, "fill parses back");
		check(fitModeFromName("stretch") == FitMode::Stretch, "stretch parses back");
		check(fitModeFromName("nonsense") == FitMode::None, "an unknown mode is off");
		check(fitModeFromName("nonsense", FitMode::Center) == FitMode::Center, "an unknown mode falls back");
		check(std::string(layoutName(Layout::SideBySide)) == "side-by-side", "layouts have names");
		check(layoutFromName("main-corner") == Layout::MainCorner, "layouts parse back");
		check(std::string(issueName(Issue::CutOff)) == "cut off", "issues have names");

		check(alignIndex(Align::BottomRight) == 8, "nine alignments, bottom right is last");
		check(alignFromIndex(0) == Align::TopLeft, "index 0 is top left");
		check(alignFromIndex(9) == Align::Center, "a bad alignment falls back to the centre");
		check(alignFromIndex(-1) == Align::Center, "a negative alignment falls back to the centre");
	}

	std::printf("rectangles\n");
	{
		check(valid(Rect{0, 0, 10, 10}), "a normal rectangle is valid");
		check(!valid(Rect{0, 0, 0, 10}), "a zero width rectangle is not");
		check(area(Rect{10, 20, 3, 4}) == 12, "area is width times height");
		check(area(Rect{0, 0, -5, 4}) == 0, "a negative rectangle has no area");

		checkRect(inset(Rect{0, 0, 100, 50}, 10), Rect{10, 10, 80, 30}, "a margin shrinks every side");
		checkRect(inset(Rect{0, 0, 100, 50}, 0), Rect{0, 0, 100, 50}, "no margin changes nothing");
		checkRect(inset(Rect{0, 0, 100, 50}, -5), Rect{0, 0, 100, 50}, "a negative margin changes nothing");
		checkRect(inset(Rect{0, 0, 3, 3}, 5), Rect{0, 0, 3, 3}, "a margin bigger than the box leaves the box");

		Item item;
		item.srcW = 1920;
		item.srcH = 1080;
		item.posX = 10.4;
		item.posY = 20.6;
		item.scaleX = 0.5;
		item.scaleY = 0.25;
		checkRect(bounds(item), Rect{10, 21, 960, 270}, "an item's box is rounded to whole pixels");

		Item unmeasured;
		checkRect(bounds(unmeasured), Rect{0, 0, 0, 0}, "a source with no size has no box");
	}

	std::printf("fit\n");
	{
		const Placement full = placeInBox(FitMode::Fit, 1920, 1080, Rect{0, 0, 1920, 1080}, Align::Center);
		check(full.ok, "a 16:9 source fits a 16:9 canvas");
		checkRect(full.rect, Rect{0, 0, 1920, 1080}, "and fills it exactly, with no seam");
		check(full.posX == 0.0 && full.posY == 0.0, "with no offset");
		check(std::fabs(full.scaleX - 1.0) < 1e-9, "and no scaling");

		const Placement fourThree = place(FitMode::Fit, 1440, 1080, 1920, 1080);
		checkRect(fourThree.rect, Rect{240, 0, 1440, 1080}, "a 4:3 camera is pillarboxed, not stretched");

		const Placement vertical = place(FitMode::Fit, 1080, 1920, 1920, 1080);
		checkRect(vertical.rect, Rect{656, 0, 608, 1080}, "a vertical camera is centred in a wide canvas");

		const Placement withMargin =
			placeInBox(FitMode::Fit, 1920, 1080, Rect{100, 100, 1720, 880}, Align::Center);
		checkRect(withMargin.rect, Rect{178, 100, 1564, 880}, "a margin is honoured on every side");

		const Placement topLeft =
			placeInBox(FitMode::Fit, 1920, 1080, Rect{100, 100, 1720, 880}, Align::TopLeft);
		checkRect(topLeft.rect, Rect{100, 100, 1564, 880}, "top left pins the source to the corner");

		const Placement bottomRight =
			placeInBox(FitMode::Fit, 1920, 1080, Rect{100, 100, 1720, 880}, Align::BottomRight);
		checkRect(bottomRight.rect, Rect{256, 100, 1564, 880}, "bottom right pins it to the other corner");

		const Placement wide = placeInBox(FitMode::Fit, 100, 50, Rect{0, 0, 1000, 1000}, Align::Center);
		checkRect(wide.rect, Rect{0, 250, 1000, 500},
			  "a wide source in a square box keeps its shape and centres");

		check(!placeInBox(FitMode::None, 1920, 1080, Rect{0, 0, 1920, 1080}, Align::Center).ok,
		      "off does nothing at all");
		check(!placeInBox(FitMode::Fit, 0, 1080, Rect{0, 0, 1920, 1080}, Align::Center).ok,
		      "a source with no size cannot be fitted");
		check(!placeInBox(FitMode::Fit, 1920, 1080, Rect{0, 0, 0, 1080}, Align::Center).ok,
		      "nothing fits into an empty box");
	}

	std::printf("fill, stretch and centre\n");
	{
		const Placement filled = placeInBox(FitMode::Fill, 1080, 1920, Rect{0, 0, 1920, 1080}, Align::Center);
		checkRect(filled.rect, Rect{0, -1166, 1920, 3413}, "fill covers the canvas and hangs over both edges");
		check(filled.rect.w == 1920, "fill makes the short side match the canvas");

		const Placement stretched = place(FitMode::Stretch, 1440, 1080, 1920, 1080);
		checkRect(stretched.rect, Rect{0, 0, 1920, 1080}, "stretch takes the whole box");
		check(std::fabs(stretched.scaleX - stretched.scaleY) > 0.01, "and does not keep the aspect ratio");

		const Placement native = placeInBox(FitMode::Center, 1000, 500, Rect{0, 0, 1920, 1080}, Align::Center);
		checkRect(native.rect, Rect{460, 290, 1000, 500}, "centre puts a source at its own size in the middle");
		check(std::fabs(native.scaleX - 1.0) < 1e-9, "centre never enlarges anything");

		const Placement shrunk = place(FitMode::Center, 3840, 2160, 1920, 1080);
		checkRect(shrunk.rect, Rect{0, 0, 1920, 1080}, "centre still shrinks a source that cannot fit");
	}

	std::printf("scale limits\n");
	{
		Placement tiny = placeInBox(FitMode::Fit, 100, 100, Rect{0, 0, 1920, 1080}, Align::Center, 0.01, 4.0);
		check(tiny.clamped, "a source that would be blown up 10x is clamped");
		checkRect(tiny.rect, Rect{760, 340, 400, 400}, "and lands at the biggest allowed size, centred");

		Placement huge =
			placeInBox(FitMode::Fit, 10000, 10000, Rect{0, 0, 1920, 1080}, Align::Center, 0.5, 20.0);
		check(huge.clamped, "a source that would be shrunk to nothing is clamped too");
		checkRect(huge.rect, Rect{-1540, -1960, 5000, 5000}, "and keeps at least half its size");

		Placement stretch =
			placeInBox(FitMode::Stretch, 100, 100, Rect{0, 0, 1920, 1080}, Align::Center, 0.01, 4.0);
		check(stretch.clamped, "stretch clamps each axis");
		checkRect(stretch.rect, Rect{760, 340, 400, 400}, "to the same limit on both");

		Placement normal = place(FitMode::Fit, 1920, 1080, 1920, 1080);
		check(!normal.clamped, "an ordinary fit is not clamped");
	}

	std::printf("audit\n");
	{
		Item perfect;
		perfect.srcW = 1920;
		perfect.srcH = 1080;
		Report report = audit(perfect, 1920, 1080);
		check(report.issue == Issue::Fine, "a source that fills the canvas is fine");
		check(std::fabs(report.coverage - 1.0) < 1e-9, "and covers all of it");

		Item offscreen = perfect;
		offscreen.posX = 2000;
		report = audit(offscreen, 1920, 1080);
		check(report.issue == Issue::OffCanvas, "a source parked beyond the right edge is off canvas");
		check(report.suggested == FitMode::Fit, "and the suggested fix is fit");

		Item offscreenLeft = perfect;
		offscreenLeft.posX = -1920;
		check(audit(offscreenLeft, 1920, 1080).issue == Issue::OffCanvas, "and so is one parked to the left");

		Item cut = perfect;
		cut.posX = -100;
		report = audit(cut, 1920, 1080);
		check(report.issue == Issue::CutOff, "a source hanging off the left edge is cut off");
		check(report.suggested == FitMode::Fit, "with fit suggested");

		Item speck = perfect;
		speck.scaleX = 0.05;
		speck.scaleY = 0.05;
		report = audit(speck, 1920, 1080);
		check(report.issue == Issue::Tiny, "a source scaled to a speck is tiny");
		check(report.coverage < 0.01, "and the coverage says so");

		Item squashed = perfect;
		squashed.scaleY = 0.5;
		report = audit(squashed, 1920, 1080);
		check(report.issue == Issue::Distorted, "a half height camera is distorted");

		Item nearly = perfect;
		nearly.scaleX = 0.99;
		check(audit(nearly, 1920, 1080).issue == Issue::Fine, "a one percent aspect drift is invisible");

		Item rotated = perfect;
		rotated.rotation = 90;
		check(audit(rotated, 1920, 1080).issue == Issue::Rotated, "a rotated source is reported, not judged");

		Item rotatedFull = perfect;
		rotatedFull.rotation = 360;
		check(audit(rotatedFull, 1920, 1080).issue == Issue::Fine, "and a full turn is not a rotation");

		Item empty;
		check(audit(empty, 1920, 1080).issue == Issue::NoSize, "a source with no size has nothing to judge");

		Item overhang = perfect;
		overhang.posX = -1;
		check(audit(overhang, 1920, 1080).issue == Issue::Fine, "one pixel over the edge is tolerated");
		overhang.posX = -3;
		check(audit(overhang, 1920, 1080).issue == Issue::CutOff, "three is not");

		/* Filling on purpose must not be reported as cut off. */
		const Placement filled = placeInBox(FitMode::Fill, 1080, 1920, Rect{0, 0, 1920, 1080}, Align::Center);
		Item fillItem = fitted(filled, 1080, 1920);
		AuditOptions fillOptions;
		check(audit(fillItem, 1920, 1080, fillOptions).issue == Issue::CutOff,
		      "a filling source looks cut off to a plain audit");
		fillOptions.adopted = FitMode::Fill;
		check(audit(fillItem, 1920, 1080, fillOptions).issue == Issue::Fine,
		      "but not to one that knows it is filling");

		/* Stretching on purpose must not be reported as distorted. */
		const Placement stretched = place(FitMode::Stretch, 1440, 1080, 1920, 1080);
		Item stretchItem = fitted(stretched, 1440, 1080);
		AuditOptions stretchOptions;
		check(audit(stretchItem, 1920, 1080, stretchOptions).issue == Issue::Distorted,
		      "a stretched source looks distorted to a plain audit");
		stretchOptions.adopted = FitMode::Stretch;
		check(audit(stretchItem, 1920, 1080, stretchOptions).issue == Issue::Fine,
		      "but not to one that knows it is stretched");
	}

	std::printf("a fixed source is a fixed source\n");
	{
		/* Every mode, every common canvas: whatever we place must pass the
		 * audit that the dock shows the user, or the plugin argues with
		 * itself. */
		const int canvases[][2] = {{1920, 1080}, {1080, 1920}, {1080, 1080}, {2560, 1440}, {1280, 720}};
		const int sources[][2] = {{1920, 1080}, {1440, 1080}, {1080, 1920},
					  {1280, 720},  {800, 600},   {3840, 2160}};

		bool allFine = true;
		for (const auto &canvas : canvases) {
			for (const auto &source : sources) {
				for (FitMode mode : {FitMode::Fit, FitMode::Center, FitMode::Stretch}) {
					AuditOptions options;
					options.adopted = mode;
					const Placement placement = placeInBox(mode, source[0], source[1],
									       Rect{0, 0, canvas[0], canvas[1]},
									       Align::Center);
					if (!placement.ok)
						continue;
					const Report report = audit(fitted(placement, source[0], source[1]), canvas[0],
								    canvas[1], options);
					if (report.issue != Issue::Fine) {
						std::printf("        %dx%d in %dx%d as %s -> %s\n", source[0],
							    source[1], canvas[0], canvas[1], fitModeName(mode),
							    issueName(report.issue));
						allFine = false;
					}
				}
			}
		}
		check(allFine, "fit, centre and stretch always pass their own audit");

		/* Centre is the one mode that can leave a small source small, which is
		 * the point of it, so it is the only one that may need a second look. */
		const Placement native = placeInBox(FitMode::Center, 160, 120, Rect{0, 0, 1920, 1080}, Align::Center);
		check(audit(fitted(native, 160, 120), 1920, 1080).issue == Issue::Tiny,
		      "and a deliberately small source is still called tiny");

		/* The placement must survive being applied to itself: running the same
		 * mode twice changes nothing, so a re-fit loop cannot drift. */
		const Placement once = place(FitMode::Fit, 1080, 1920, 1920, 1080);
		const Rect firstBox = once.rect;
		const Placement twice = placeInBox(FitMode::Fit, 1080, 1920, Rect{0, 0, 1920, 1080}, Align::Center);
		check(sameRect(firstBox, twice.rect), "fitting twice gives the same answer");
	}

	std::printf("splitting the canvas\n");
	{
		const std::vector<Span> three = split(1920, 3, 8);
		check(three.size() == 3, "three columns");
		check(three[0].start == 0, "the first starts at zero");
		check(three[0].size + three[1].size + three[2].size + 16 == 1920, "the widths and gaps add up exactly");
		check(three[1].start == three[0].size + 8, "with the gap between them");
		check(three[2].start + three[2].size == 1920, "and the last one ends on the edge");
		check(three[0].size >= three[2].size, "spare pixels go to the left");

		const std::vector<Span> one = split(1920, 1, 8);
		check(one.size() == 1 && one[0].size == 1920, "one part is the whole thing");

		const std::vector<Span> none = split(1920, 0, 8);
		check(none.empty(), "no parts is nothing");

		const std::vector<Span> empty = split(0, 3, 8);
		check(empty.empty(), "no space is nothing");

		const std::vector<Span> greedy = split(10, 3, 10);
		check(greedy.size() == 3, "gaps wider than the canvas still make three parts");
		check(greedy[0].size + greedy[1].size + greedy[2].size <= 10, "that fit in the space");
		check(greedy[2].start + greedy[2].size <= 10, "and never overflow it");
	}

	std::printf("layouts\n");
	{
		check(layoutCells(Layout::None, 3, 1920, 1080, 8).empty(), "no layout makes no cells");
		check(layoutCells(Layout::Grid, 0, 1920, 1080, 8).empty(), "no sources makes no cells");

		const std::vector<Rect> single = layoutCells(Layout::Grid, 1, 1920, 1080, 8);
		check(single.size() == 1, "one source, one cell");
		checkRect(single[0], Rect{0, 0, 1920, 1080}, "and it gets the whole canvas");

		const std::vector<Rect> pair = layoutCells(Layout::Grid, 2, 1920, 1080, 8);
		check(pair.size() == 2, "two sources, two cells");
		checkRect(pair[0], Rect{0, 0, 956, 1080}, "side by side, half each");
		checkRect(pair[1], Rect{964, 0, 956, 1080}, "with a gap between");
		check(!overlaps(pair[0], pair[1]), "that do not overlap");

		const std::vector<Rect> four = layoutCells(Layout::Grid, 4, 1920, 1080, 8);
		check(four.size() == 4, "four sources, four cells");
		check(noOverlaps(four), "none of them overlap");
		check(allInside(four, 1920, 1080), "all of them are on the canvas");
		checkRect(four[0], Rect{0, 0, 956, 536}, "the first is the top left quadrant");
		checkRect(four[3], Rect{964, 544, 956, 536}, "the last is the bottom right");

		const std::vector<Rect> three = layoutCells(Layout::Grid, 3, 1920, 1080, 8);
		check(three.size() == 3, "three sources, three cells");
		check(noOverlaps(three) && allInside(three, 1920, 1080), "that stay apart and on screen");
		checkRect(three[2], Rect{482, 544, 956, 536}, "the odd one out is centred on its own row");

		const std::vector<Rect> five = layoutCells(Layout::Grid, 5, 1920, 1080, 8);
		check(five.size() == 5, "five sources, five cells");
		check(noOverlaps(five) && allInside(five, 1920, 1080), "that stay apart and on screen");

		const std::vector<Rect> row = layoutCells(Layout::SideBySide, 3, 1920, 1080, 8);
		check(row.size() == 3, "three across makes three columns");
		check(row[0].h == 1080 && row[2].h == 1080, "full height");
		check(row[2].x + row[2].w == 1920, "reaching the right edge");
		check(noOverlaps(row), "without overlap");

		const std::vector<Rect> main = layoutCells(Layout::MainCorner, 3, 1920, 1080, 8);
		check(main.size() == 3, "main plus two cams makes three cells");
		checkRect(main[0], Rect{0, 0, 1920, 1080}, "the main source keeps the canvas");
		check(main[1].w == main[1].h, "the cams are square");
		check(allInside(main, 1920, 1080), "the cams sit on the canvas");
		check(main[2].y < main[1].y, "stacked upwards from the corner");
		check(main[1].x + main[1].w <= 1920 - 7, "hugging the right edge with a gap");

		const std::vector<Rect> many = layoutCells(Layout::MainCorner, 8, 1920, 1080, 8);
		check(many.size() == 8, "eight sources still get eight cells");
		check(allInside(many, 1920, 1080), "and the corner column still fits");
		check(!overlaps(many[1], many[7]), "the cams do not sit on each other");
	}

	std::printf("when to re-fit\n");
	{
		const Watch now{1920, 1080, 1920, 1080};
		check(!needsRefit(FitMode::None, now, now), "a source nobody looks after is never re-fitted");
		check(!needsRefit(FitMode::Fit, now, now), "a source that did not change is left alone");

		Watch wider = now;
		wider.canvasW = 1080;
		wider.canvasH = 1920;
		check(needsRefit(FitMode::Fit, now, wider), "a canvas that became vertical needs a re-fit");

		Watch browser = now;
		browser.srcW = 1280;
		browser.srcH = 720;
		check(needsRefit(FitMode::Fill, now, browser), "a browser source that changed resolution too");

		const Watch unknown{0, 0, 1920, 1080};
		check(!needsRefit(FitMode::Fit, unknown, unknown), "a source we have never measured is not a change");
	}

	std::printf("\n%s (%d checks, %d failure%s)\n", g_failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", g_checks,
		    g_failures, g_failures == 1 ? "" : "s");

	return g_failures == 0 ? 0 : 1;
}
