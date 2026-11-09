/*
Export Kit for Photoshop
Copyright (C) 2026 Emmanuel Suah

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
 * Where to crop a wide photo into a vertical story, without cutting the face
 * out of it.
 *
 * There is no machine learning here and no claim to any: the plugin reads the
 * document at a few dozen pixels across and looks for the place with the most
 * *detail* — edges and colour — because that is where the subject is and where
 * a flat background is not. A blurred sky, a radial gradient and a studio
 * backdrop all score close to zero, which is the correct answer for them.
 *
 * Everything in this file is plain arithmetic over an energy map, so it is
 * tested directly: see tests/smartcrop.test.js.
 */

"use strict";

/* Rec. 709 luminance, 0..255 in, 0..255 out. */
function luminance(r, g, b) {
	return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

/*
 * How "interesting" each cell of a downscaled image is: how fast the brightness
 * changes (edges, text, faces, product detail) plus how colourful it is (a red
 * game character beats a grey wall at the same contrast).
 */
function energyMapFromPixels(data, width, height, components) {
	const cells = width * height;
	const cellsPerPixel = components || 4;
	const luma = new Float32Array(cells);
	const energy = new Float32Array(cells);

	if (!data || !Number.isFinite(width) || !Number.isFinite(height) || width <= 0 || height <= 0)
		return { width: 0, height: 0, energy };

	for (let i = 0; i < cells; i++) {
		const base = i * cellsPerPixel;
		luma[i] = luminance(data[base] || 0, data[base + 1] || 0, data[base + 2] || 0);
	}

	for (let y = 0; y < height; y++) {
		for (let x = 0; x < width; x++) {
			const index = y * width + x;
			const left = luma[y * width + Math.max(0, x - 1)];
			const right = luma[y * width + Math.min(width - 1, x + 1)];
			const up = luma[Math.max(0, y - 1) * width + x];
			const down = luma[Math.min(height - 1, y + 1) * width + x];

			const gradient = (Math.abs(right - left) + Math.abs(down - up)) / 2;

			const base = index * cellsPerPixel;
			const r = data[base] || 0;
			const g = data[base + 1] || 0;
			const b = data[base + 2] || 0;
			const max = Math.max(r, g, b);
			const min = Math.min(r, g, b);
			const chroma = max - min;

			energy[index] = gradient + chroma * 0.35;
		}
	}

	return { width, height, energy };
}

/*
 * The size of the crop window, in cells, for a given target aspect ratio.
 * The window always keeps the slot's shape — that is the whole point.
 */
function windowCells(mapWidth, mapHeight, aspect) {
	const ratio = Number.isFinite(aspect) && aspect > 0 ? aspect : 1;

	let width = mapWidth;
	let height = Math.round(mapWidth / ratio);

	if (height > mapHeight) {
		height = mapHeight;
		width = Math.round(mapHeight * ratio);
	}

	return {
		width: Math.max(1, Math.min(mapWidth, width)),
		height: Math.max(1, Math.min(mapHeight, height)),
	};
}

/* Summed area table, so every candidate window costs four reads. */
function integral(energy, width, height) {
	const stride = width + 1;
	const table = new Float64Array(stride * (height + 1));

	for (let y = 0; y < height; y++) {
		let rowTotal = 0;
		for (let x = 0; x < width; x++) {
			rowTotal += energy[y * width + x];
			table[(y + 1) * stride + (x + 1)] = table[y * stride + (x + 1)] + rowTotal;
		}
	}

	return { table, stride };
}

function windowScore(summed, x, y, width, height) {
	const { table, stride } = summed;
	const right = x + width;
	const bottom = y + height;

	return table[bottom * stride + right] - table[y * stride + right] - table[bottom * stride + x] + table[y * stride + x];
}

/*
 * Finds the crop window with the most detail in it.
 *
 * A gentle centre bias keeps a tie (or a featureless photo, where every window
 * scores the same) in the middle instead of at a random corner — the boring
 * answer is the right one when the picture gives no reason to move.
 */
function bestWindow(energy, mapWidth, mapHeight, windowWidth, windowHeight, options) {
	const bias = options && Number.isFinite(options.centreBias) ? options.centreBias : 0.15;

	const maxX = Math.max(0, mapWidth - windowWidth);
	const maxY = Math.max(0, mapHeight - windowHeight);

	if (maxX === 0 && maxY === 0)
		return { x: 0, y: 0, score: 0, centreScore: 0, confidence: 0 };

	const summed = integral(energy, mapWidth, mapHeight);

	const centreX = Math.floor(maxX / 2);
	const centreY = Math.floor(maxY / 2);
	const centreScore = windowScore(summed, centreX, centreY, windowWidth, windowHeight);

	/* Start at the centre rather than at the first window: when nothing stands
	 * out every candidate scores the same, and the boring answer (the middle)
	 * is the correct one. Without this the winner would be a corner. */
	let best = { x: centreX, y: centreY, score: centreScore, raw: centreScore };

	for (let y = 0; y <= maxY; y++) {
		for (let x = 0; x <= maxX; x++) {
			const raw = windowScore(summed, x, y, windowWidth, windowHeight);

			/* How far from the middle this window sits, 0..1. */
			const dx = maxX > 0 ? Math.abs(x - maxX / 2) / (maxX / 2) : 0;
			const dy = maxY > 0 ? Math.abs(y - maxY / 2) / (maxY / 2) : 0;
			const distance = Math.min(1, Math.sqrt(dx * dx + dy * dy) / Math.SQRT2);

			const score = raw * (1 - bias * distance);

			if (score > best.score) {
				best = { x, y, score, raw };
			}
		}
	}

	/* How much better the winner is than just cropping the middle, on a 0..1
	 * scale. A centre with no detail at all and a subject that has plenty is
	 * the most certain answer there is, not a division by zero. */
	const winnerRaw = best.raw === undefined ? best.score : best.raw;
	const scale = Math.max(1e-6, Math.max(winnerRaw, centreScore));
	const confidence = Math.max(0, Math.min(1, (winnerRaw - centreScore) / scale));

	return { x: best.x, y: best.y, score: best.score, centreScore, confidence };
}

/* Maps a cell offset onto the pixels the resize actually left over. */
function offsetToPixels(cellOffset, cellSpan, slackPixels) {
	const slack = Math.max(0, Math.round(slackPixels || 0));
	if (slack === 0 || !Number.isFinite(cellSpan) || cellSpan <= 0)
		return 0;

	const ratio = Math.min(1, Math.max(0, cellOffset / cellSpan));
	return Math.round(ratio * slack);
}

/*
 * The whole decision for one job: read the energy, pick a window, turn it back
 * into pixels. Returns the crop offset to hand to withCropOffset(), plus how
 * sure the choice is, so the panel can say "cropped smartly" or "kept centred".
 */
function chooseCrop(options) {
	const {
		data,
		mapWidth,
		mapHeight,
		components,
		aspect,
		slackX,
		slackY,
		minConfidence = 0.08,
		centreBias,
	} = options || {};

	const map = energyMapFromPixels(data, mapWidth, mapHeight, components);
	if (!map.width || !map.height)
		return { offsetX: 0, offsetY: 0, confidence: 0, smart: false };

	const window = windowCells(map.width, map.height, aspect);
	const best = bestWindow(map.energy, map.width, map.height, window.width, window.height, { centreBias });

	const spanX = map.width - window.width;
	const spanY = map.height - window.height;

	const smart = best.confidence >= minConfidence;

	const offsetX = smart ? offsetToPixels(best.x, spanX, slackX) : Math.round((slackX || 0) / 2);
	const offsetY = smart ? offsetToPixels(best.y, spanY, slackY) : Math.round((slackY || 0) / 2);

	return {
		offsetX,
		offsetY,
		confidence: best.confidence,
		smart,
		window: { width: window.width, height: window.height },
	};
}

module.exports = {
	luminance,
	energyMapFromPixels,
	windowCells,
	bestWindow,
	offsetToPixels,
	chooseCrop,
};
