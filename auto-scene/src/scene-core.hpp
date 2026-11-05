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

#pragma once

#include <string>
#include <vector>

/*
 * The arithmetic behind Auto Scene Adjust.
 *
 * Nothing here knows about OBS or Qt: it is rectangles, aspect ratios and a
 * few rules about what "this source looks wrong" means. That is on purpose —
 * it is the part that can be tested, and the part that must not be wrong,
 * because a bug here moves somebody's camera in the middle of a stream.
 *
 * Coordinates are canvas pixels with the origin at the top left. A scene
 * item's transform is reduced to "the top-left corner of the drawn box plus
 * the scale applied to the source's intrinsic size", which is what OBS calls
 * alignment 0 (left|top).
 */
namespace autoscene {

/* What to do with a source that is in the wrong place. */
enum class FitMode {
	None = 0, /* leave it alone */
	Fit,      /* whole source visible, letterboxed, aspect kept */
	Fill,     /* covers the box, overflow cropped, aspect kept */
	Center,   /* native size, centred, shrunk only if it cannot fit */
	Stretch,  /* exact box, aspect not kept */
};

/* Where the result sits inside its box. */
enum class Align {
	TopLeft = 0,
	TopCenter,
	TopRight,
	MiddleLeft,
	Center,
	MiddleRight,
	BottomLeft,
	BottomCenter,
	BottomRight,
};

/* Layout presets: one click ways to arrange the sources of a scene. */
enum class Layout {
	None = 0,
	Grid,
	SideBySide,
	MainCorner,
};

/* What is wrong with a source, if anything. */
enum class Issue {
	Fine = 0,
	NoSize,    /* no intrinsic size or zero scale: nothing to reason about */
	Rotated,   /* rotated items are reported and never touched */
	OffCanvas, /* entirely outside the canvas: invisible */
	CutOff,    /* hangs off an edge */
	Tiny,      /* technically on screen, but a speck */
	Distorted, /* aspect ratio does not match the source */
};

const char *fitModeName(FitMode mode);
FitMode fitModeFromName(const std::string &name, FitMode fallback = FitMode::None);

const char *alignName(Align align);
Align alignFromIndex(int index, Align fallback = Align::Center);
int alignIndex(Align align);

const char *layoutName(Layout layout);
Layout layoutFromName(const std::string &name, Layout fallback = Layout::None);

const char *issueName(Issue issue);

/* ------------------------------------------------------------------------- */
/* rectangles                                                                */
/* ------------------------------------------------------------------------- */

struct Rect {
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

bool valid(const Rect &rect);
long long area(const Rect &rect);

/* Shrinks every side by `margin`, never below one pixel. */
Rect inset(const Rect &rect, int margin);

/* What a source currently occupies, as far as the maths is concerned. */
struct Item {
	int srcW = 0;
	int srcH = 0;
	double posX = 0.0;
	double posY = 0.0;
	double scaleX = 1.0;
	double scaleY = 1.0;
	int rotation = 0; /* degrees */
};

/* The drawn box of an item, rounded to whole pixels. */
Rect bounds(const Item &item);

/* ------------------------------------------------------------------------- */
/* fitting                                                                   */
/* ------------------------------------------------------------------------- */

struct Placement {
	double posX = 0.0;
	double posY = 0.0;
	double scaleX = 1.0;
	double scaleY = 1.0;
	Rect rect;
	bool ok = false;      /* false when there is nothing to compute */
	bool clamped = false; /* the scale limits got in the way */
};

struct PlacementOptions {
	int margin = 0;
	Align align = Align::Center;
	double minScale = 0.01;
	double maxScale = 20.0;
};

/* Places a source inside an arbitrary box (a canvas, a cell of a layout...). */
Placement placeInBox(FitMode mode, int srcW, int srcH, const Rect &box, Align align, double minScale = 0.01,
		     double maxScale = 20.0);

/* Places a source in the canvas, honouring `options.margin`. */
Placement place(FitMode mode, int srcW, int srcH, int canvasW, int canvasH, const PlacementOptions &options = {});

/* ------------------------------------------------------------------------- */
/* audit                                                                     */
/* ------------------------------------------------------------------------- */

struct AuditOptions {
	/* How far the on-screen aspect may drift from the source's before it
	 * counts as squashed. 2% is invisible; 20% is a stretched camera. */
	double aspectTolerance = 0.02;
	/* Below this share of the canvas a source counts as a speck. */
	double tinyCoverage = 0.01;
	/* A source may sit this many pixels over an edge before we call it. */
	int edgeTolerance = 1;
	/* The mode this source is managed by, if any. Fill is supposed to crop
	 * and Stretch is supposed to squash, so the audit must not complain about
	 * the very thing the user asked for. */
	FitMode adopted = FitMode::None;
};

struct Report {
	Rect rect;
	double coverage = 0.0; /* share of the canvas covered, may exceed 1 */
	Issue issue = Issue::Fine;
	/* The single fix most likely to help, or None when nothing will. */
	FitMode suggested = FitMode::None;
};

Report audit(const Item &item, int canvasW, int canvasH, const AuditOptions &options = {});

/* ------------------------------------------------------------------------- */
/* layout presets                                                            */
/* ------------------------------------------------------------------------- */

/* One cell of a split, in canvas pixels. */
struct Span {
	int start = 0;
	int size = 0;
};

/* Splits `total` into `count` spans separated by `gap`, exactly: the sizes add
 * up to `total` with no leftover pixel, and the spare pixels (at most
 * count - 1) go to the left-most spans. */
std::vector<Span> split(int total, int count, int gap);

/* Cells for `count` sources. Empty when the layout is None or nothing fits. */
std::vector<Rect> layoutCells(Layout layout, int count, int canvasW, int canvasH, int gap);

/* ------------------------------------------------------------------------- */
/* when to re-fit automatically                                              */
/* ------------------------------------------------------------------------- */

/* What the plugin remembers about a source it looks after. */
struct Watch {
	int srcW = 0;
	int srcH = 0;
	int canvasW = 0;
	int canvasH = 0;
};

/* True when the source or the canvas changed size since the last look, which
 * is exactly when an adopted source has to be placed again. */
bool needsRefit(FitMode mode, const Watch &last, const Watch &now);

} // namespace autoscene
