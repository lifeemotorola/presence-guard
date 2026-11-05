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

#include "scene-core.hpp"

#include <algorithm>
#include <cmath>

namespace autoscene {

/* ------------------------------------------------------------------------- */
/* names                                                                     */
/* ------------------------------------------------------------------------- */

const char *fitModeName(FitMode mode)
{
	switch (mode) {
	case FitMode::Fit:
		return "fit";
	case FitMode::Fill:
		return "fill";
	case FitMode::Center:
		return "center";
	case FitMode::Stretch:
		return "stretch";
	case FitMode::None:
		break;
	}
	return "off";
}

FitMode fitModeFromName(const std::string &name, FitMode fallback)
{
	if (name == "fit")
		return FitMode::Fit;
	if (name == "fill")
		return FitMode::Fill;
	if (name == "center")
		return FitMode::Center;
	if (name == "stretch")
		return FitMode::Stretch;
	if (name == "off")
		return FitMode::None;
	return fallback;
}

const char *alignName(Align align)
{
	switch (align) {
	case Align::TopLeft:
		return "top left";
	case Align::TopCenter:
		return "top";
	case Align::TopRight:
		return "top right";
	case Align::MiddleLeft:
		return "left";
	case Align::Center:
		return "centre";
	case Align::MiddleRight:
		return "right";
	case Align::BottomLeft:
		return "bottom left";
	case Align::BottomCenter:
		return "bottom";
	case Align::BottomRight:
		return "bottom right";
	}
	return "centre";
}

Align alignFromIndex(int index, Align fallback)
{
	if (index < 0 || index > 8)
		return fallback;
	return static_cast<Align>(index);
}

int alignIndex(Align align)
{
	return static_cast<int>(align);
}

const char *layoutName(Layout layout)
{
	switch (layout) {
	case Layout::Grid:
		return "grid";
	case Layout::SideBySide:
		return "side-by-side";
	case Layout::MainCorner:
		return "main-corner";
	case Layout::None:
		break;
	}
	return "none";
}

Layout layoutFromName(const std::string &name, Layout fallback)
{
	if (name == "grid")
		return Layout::Grid;
	if (name == "side-by-side")
		return Layout::SideBySide;
	if (name == "main-corner")
		return Layout::MainCorner;
	if (name == "none")
		return Layout::None;
	return fallback;
}

const char *issueName(Issue issue)
{
	switch (issue) {
	case Issue::Fine:
		return "fine";
	case Issue::NoSize:
		return "no size";
	case Issue::Rotated:
		return "rotated";
	case Issue::OffCanvas:
		return "off canvas";
	case Issue::CutOff:
		return "cut off";
	case Issue::Tiny:
		return "tiny";
	case Issue::Distorted:
		return "distorted";
	}
	return "fine";
}

/* ------------------------------------------------------------------------- */
/* rectangles                                                                */
/* ------------------------------------------------------------------------- */

bool valid(const Rect &rect)
{
	return rect.w > 0 && rect.h > 0;
}

long long area(const Rect &rect)
{
	if (rect.w <= 0 || rect.h <= 0)
		return 0;
	return static_cast<long long>(rect.w) * static_cast<long long>(rect.h);
}

Rect inset(const Rect &rect, int margin)
{
	Rect result = rect;
	if (margin <= 0)
		return result;

	/* A margin bigger than the canvas would make the box meaningless, so it
	 * collapses to a single pixel instead of a negative rectangle. */
	const int doubled = margin * 2;
	if (doubled >= rect.w || doubled >= rect.h) {
		result.w = std::max(1, rect.w);
		result.h = std::max(1, rect.h);
		return result;
	}

	result.x += margin;
	result.y += margin;
	result.w -= doubled;
	result.h -= doubled;
	return result;
}

Rect bounds(const Item &item)
{
	Rect rect;

	if (item.srcW <= 0 || item.srcH <= 0)
		return rect;

	rect.x = static_cast<int>(std::lround(item.posX));
	rect.y = static_cast<int>(std::lround(item.posY));
	rect.w = static_cast<int>(std::lround(item.srcW * item.scaleX));
	rect.h = static_cast<int>(std::lround(item.srcH * item.scaleY));
	return rect;
}

/* ------------------------------------------------------------------------- */
/* fitting                                                                   */
/* ------------------------------------------------------------------------- */

static int alignedOffset(int available, int used, int position)
{
	/* position: 0 = start, 1 = middle, 2 = end */
	switch (position) {
	case 0:
		return 0;
	case 2:
		return available - used;
	default:
		return (available - used) / 2;
	}
}

Placement placeInBox(FitMode mode, int srcW, int srcH, const Rect &box, Align align, double minScale, double maxScale)
{
	Placement result;

	if (mode == FitMode::None)
		return result;
	if (srcW <= 0 || srcH <= 0)
		return result;
	if (box.w <= 0 || box.h <= 0)
		return result;

	const double boxW = static_cast<double>(box.w);
	const double boxH = static_cast<double>(box.h);
	const double srcWf = static_cast<double>(srcW);
	const double srcHf = static_cast<double>(srcH);

	double scaleX = 1.0;
	double scaleY = 1.0;

	switch (mode) {
	case FitMode::Fit:
		scaleX = scaleY = std::min(boxW / srcWf, boxH / srcHf);
		break;
	case FitMode::Fill:
		scaleX = scaleY = std::max(boxW / srcWf, boxH / srcHf);
		break;
	case FitMode::Center:
		/* Native size, shrunk only when the source is bigger than the box:
		 * "put it back how it was" must never enlarge anything. */
		scaleX = scaleY = std::min(1.0, std::min(boxW / srcWf, boxH / srcHf));
		break;
	case FitMode::Stretch:
		scaleX = boxW / srcWf;
		scaleY = boxH / srcHf;
		break;
	case FitMode::None:
		return result;
	}

	const double factor = std::max(0.0001, std::min(minScale, maxScale));

	if (mode == FitMode::Stretch) {
		const double low = factor;
		const double high = std::max(factor, maxScale);
		const double clampedX = std::max(low, std::min(high, scaleX));
		const double clampedY = std::max(low, std::min(high, scaleY));
		result.clamped = clampedX != scaleX || clampedY != scaleY;
		scaleX = clampedX;
		scaleY = clampedY;
	} else {
		const double low = factor;
		const double high = std::max(factor, maxScale);
		const double clamped = std::max(low, std::min(high, scaleX));
		result.clamped = clamped != scaleX;
		scaleX = clamped;
		scaleY = clamped;
	}

	int width = static_cast<int>(std::lround(srcWf * scaleX));
	int height = static_cast<int>(std::lround(srcHf * scaleY));
	width = std::max(1, width);
	height = std::max(1, height);

	/* Keep the scale and the pixel box in agreement: the dock shows the box,
	 * OBS applies the scale, and a one pixel mismatch between them is a seam
	 * nobody can explain later. */
	scaleX = static_cast<double>(width) / srcWf;
	scaleY = static_cast<double>(height) / srcHf;

	const int horizontal = (static_cast<int>(align) % 3);
	const int vertical = (static_cast<int>(align) / 3);

	const int offsetX = alignedOffset(box.w, width, horizontal);
	const int offsetY = alignedOffset(box.h, height, vertical);

	result.posX = static_cast<double>(box.x + offsetX);
	result.posY = static_cast<double>(box.y + offsetY);
	result.scaleX = scaleX;
	result.scaleY = scaleY;
	result.rect = Rect{box.x + offsetX, box.y + offsetY, width, height};
	result.ok = true;
	return result;
}

Placement place(FitMode mode, int srcW, int srcH, int canvasW, int canvasH, const PlacementOptions &options)
{
	const Rect canvas{0, 0, canvasW, canvasH};
	return placeInBox(mode, srcW, srcH, inset(canvas, options.margin), options.align, options.minScale,
			  options.maxScale);
}

/* ------------------------------------------------------------------------- */
/* audit                                                                     */
/* ------------------------------------------------------------------------- */

static FitMode suggestedFix(Issue issue)
{
	switch (issue) {
	case Issue::OffCanvas:
	case Issue::CutOff:
	case Issue::Tiny:
	case Issue::Distorted:
		return FitMode::Fit;
	case Issue::Fine:
	case Issue::NoSize:
	case Issue::Rotated:
		break;
	}
	return FitMode::None;
}

Report audit(const Item &item, int canvasW, int canvasH, const AuditOptions &options)
{
	Report report;
	report.rect = bounds(item);

	if (item.srcW <= 0 || item.srcH <= 0 || item.scaleX <= 0.0 || item.scaleY <= 0.0) {
		report.issue = Issue::NoSize;
		return report;
	}

	if (canvasW <= 0 || canvasH <= 0) {
		report.issue = Issue::NoSize;
		return report;
	}

	report.coverage =
		static_cast<double>(area(report.rect)) / (static_cast<double>(canvasW) * static_cast<double>(canvasH));

	int rotation = item.rotation % 360;
	if (rotation < 0)
		rotation += 360;
	if (rotation != 0) {
		/* A rotated box is not a box; leave the judgement to the human. */
		report.issue = Issue::Rotated;
		return report;
	}

	const Rect &rect = report.rect;
	const int tol = std::max(0, options.edgeTolerance);

	const bool outsideLeft = rect.x + rect.w <= tol;
	const bool outsideRight = rect.x >= canvasW - tol;
	const bool outsideTop = rect.y + rect.h <= tol;
	const bool outsideBottom = rect.y >= canvasH - tol;

	if (outsideLeft || outsideRight || outsideTop || outsideBottom) {
		report.issue = Issue::OffCanvas;
		report.suggested = suggestedFix(report.issue);
		return report;
	}

	bool overLeft = rect.x < -tol;
	bool overTop = rect.y < -tol;
	bool overRight = rect.x + rect.w > canvasW + tol;
	bool overBottom = rect.y + rect.h > canvasH + tol;

	/* Filling the frame means hanging over the edge on purpose. */
	if (options.adopted == FitMode::Fill)
		overLeft = overTop = overRight = overBottom = false;

	if (overLeft || overTop || overRight || overBottom) {
		report.issue = Issue::CutOff;
		report.suggested = suggestedFix(report.issue);
		return report;
	}

	if (report.coverage < options.tinyCoverage) {
		report.issue = Issue::Tiny;
		report.suggested = suggestedFix(report.issue);
		return report;
	}

	const double sourceAspect = static_cast<double>(item.srcW) / static_cast<double>(item.srcH);
	const double drawnAspect = static_cast<double>(rect.w) / static_cast<double>(rect.h);
	const double drift = std::fabs(drawnAspect / sourceAspect - 1.0);

	if (drift > options.aspectTolerance && options.adopted != FitMode::Stretch) {
		report.issue = Issue::Distorted;
		report.suggested = suggestedFix(report.issue);
		return report;
	}

	report.issue = Issue::Fine;
	return report;
}

/* ------------------------------------------------------------------------- */
/* layout presets                                                            */
/* ------------------------------------------------------------------------- */

std::vector<Span> split(int total, int count, int gap)
{
	std::vector<Span> spans;
	if (count <= 0 || total <= 0)
		return spans;

	if (gap < 0)
		gap = 0;

	const int gapTotal = gap * (count - 1);
	int available = total - gapTotal;

	/* The gaps alone are wider than the space: shrink them rather than hand
	 * back cells that overlap. */
	if (available < count) {
		gap = count > 1 ? std::max(0, (total - count) / (count - 1)) : 0;
		available = total - gap * (count - 1);
		if (available < count)
			available = count;
	}

	const int base = available / count;
	const int extra = available % count;

	int cursor = 0;
	for (int i = 0; i < count; i++) {
		Span span;
		span.start = cursor;
		span.size = base + (i < extra ? 1 : 0);
		cursor += span.size + gap;
		spans.push_back(span);
	}

	return spans;
}

static std::vector<Rect> gridCells(int count, int canvasW, int canvasH, int gap)
{
	std::vector<Rect> cells;
	if (count <= 0)
		return cells;

	int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(count))));
	if (columns < 1)
		columns = 1;
	if (columns > count)
		columns = count;
	const int rows = (count + columns - 1) / columns;

	const std::vector<Span> cols = split(canvasW, columns, gap);
	const std::vector<Span> rowSpans = split(canvasH, rows, gap);

	for (int row = 0; row < rows; row++) {
		const int first = row * columns;
		const int inRow = std::min(columns, count - first);
		if (inRow <= 0)
			break;

		/* A short last row is centred: three sources look arranged rather
		 * than abandoned. */
		int used = 0;
		for (int i = 0; i < inRow; i++)
			used += cols[static_cast<size_t>(i)].size;
		used += gap * (inRow - 1);
		const int offset = inRow < columns ? (canvasW - used) / 2 : 0;

		for (int col = 0; col < inRow; col++) {
			Rect cell;
			cell.x = cols[static_cast<size_t>(col)].start + offset;
			cell.y = rowSpans[static_cast<size_t>(row)].start;
			cell.w = cols[static_cast<size_t>(col)].size;
			cell.h = rowSpans[static_cast<size_t>(row)].size;
			cells.push_back(cell);
		}
	}

	return cells;
}

static std::vector<Rect> sideBySideCells(int count, int canvasW, int canvasH, int gap)
{
	std::vector<Rect> cells;
	const std::vector<Span> cols = split(canvasW, count, gap);
	for (const Span &span : cols)
		cells.push_back(Rect{span.start, 0, span.size, canvasH});
	return cells;
}

static std::vector<Rect> mainCornerCells(int count, int canvasW, int canvasH, int gap)
{
	std::vector<Rect> cells;
	cells.push_back(Rect{0, 0, canvasW, canvasH});
	if (count <= 1)
		return cells;

	const int extras = count - 1;
	const int shortest = std::min(canvasW, canvasH);

	int size = static_cast<int>(std::lround(static_cast<double>(shortest) * 0.28));
	/* The whole corner column has to fit above the bottom edge, gaps and all. */
	const int roomForSize = (canvasH - gap * (extras + 1)) / extras;
	if (roomForSize > 0 && size > roomForSize)
		size = roomForSize;
	size = std::max(24, std::min(size, shortest / 2));

	const int x = canvasW - gap - size;
	for (int i = 0; i < extras; i++) {
		const int y = canvasH - gap - size - i * (size + gap);
		cells.push_back(Rect{x, y, size, size});
	}

	return cells;
}

std::vector<Rect> layoutCells(Layout layout, int count, int canvasW, int canvasH, int gap)
{
	if (count <= 0 || canvasW <= 0 || canvasH <= 0)
		return {};

	if (gap < 0)
		gap = 0;

	switch (layout) {
	case Layout::Grid:
		return gridCells(count, canvasW, canvasH, gap);
	case Layout::SideBySide:
		return sideBySideCells(count, canvasW, canvasH, gap);
	case Layout::MainCorner:
		return mainCornerCells(count, canvasW, canvasH, gap);
	case Layout::None:
		break;
	}

	return {};
}

/* ------------------------------------------------------------------------- */
/* when to re-fit automatically                                              */
/* ------------------------------------------------------------------------- */

bool needsRefit(FitMode mode, const Watch &last, const Watch &now)
{
	if (mode == FitMode::None)
		return false;

	if (now.srcW <= 0 || now.srcH <= 0)
		return false;

	/* A source that has just appeared has never been placed by us: the plugin
	 * treats "0" as "unknown" so a fresh scene does not get re-fitted on sight
	 * unless the canvas itself changed. */
	return now.srcW != last.srcW || now.srcH != last.srcH || now.canvasW != last.canvasW ||
	       now.canvasH != last.canvasH;
}

} // namespace autoscene
