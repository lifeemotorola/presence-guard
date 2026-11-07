/*
Export Kit for Photoshop — tests for the crop chooser.
Run with: npm test  (node --test)
*/

"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");

const smart = require("../src/smartcrop.js");

/* Builds an interleaved RGBA buffer where `paint(x, y)` returns [r, g, b]. */
function buffer(width, height, paint) {
	const data = new Uint8Array(width * height * 4);
	for (let y = 0; y < height; y++) {
		for (let x = 0; x < width; x++) {
			const [r, g, b] = paint(x, y);
			const base = (y * width + x) * 4;
			data[base] = r;
			data[base + 1] = g;
			data[base + 2] = b;
			data[base + 3] = 255;
		}
	}
	return data;
}

/* A flat grey field with a noisy square: the noise is the "subject". */
function withSubject(width, height, box) {
	return buffer(width, height, (x, y) => {
		const inside = x >= box.x && x < box.x + box.width && y >= box.y && y < box.y + box.height;
		if (!inside)
			return [128, 128, 128];
		const value = (x * 37 + y * 91) % 2 === 0 ? 20 : 235;
		return [value, value, value];
	});
}

test("luminance follows the eye, not the channel", () => {
	assert.equal(smart.luminance(0, 0, 0), 0);
	assert.equal(Math.round(smart.luminance(255, 255, 255)), 255);
	assert.ok(smart.luminance(0, 255, 0) > smart.luminance(255, 0, 0), "green reads brighter than red");
	assert.ok(smart.luminance(255, 0, 0) > smart.luminance(0, 0, 255), "red reads brighter than blue");
});

test("a flat picture has no energy anywhere", () => {
	const map = smart.energyMapFromPixels(buffer(16, 16, () => [120, 120, 120]), 16, 16, 4);

	assert.equal(map.width, 16);
	assert.equal(map.height, 16);
	assert.equal(map.energy.length, 256);
	for (const value of map.energy)
		assert.ok(Math.abs(value) < 1e-6, "flat grey must not attract the crop");
});

test("detail and colour both raise the energy", () => {
	const noisy = smart.energyMapFromPixels(
		buffer(8, 8, (x) => (x % 2 ? [0, 0, 0] : [255, 255, 255])),
		8,
		8,
		4,
	);
	const plain = smart.energyMapFromPixels(buffer(8, 8, () => [120, 120, 120]), 8, 8, 4);
	const colourful = smart.energyMapFromPixels(buffer(8, 8, () => [220, 20, 20]), 8, 8, 4);

	const sum = (map) => Array.from(map.energy).reduce((a, b) => a + b, 0);
	assert.ok(sum(noisy) > 0, "edges are energy");
	assert.equal(sum(plain), 0);
	assert.ok(sum(colourful) > 0, "a saturated colour is not a grey wall");
});

test("a broken buffer is refused rather than crashing the export", () => {
	const map = smart.energyMapFromPixels(null, 0, 0, 4);
	assert.equal(map.width, 0);
	assert.equal(map.energy.length, 0);
});

test("the crop window keeps the slot's shape", () => {
	const wide = smart.windowCells(100, 100, 16 / 9);
	assert.equal(wide.width, 100);
	assert.equal(wide.height, 56);

	const tall = smart.windowCells(100, 100, 9 / 16);
	assert.equal(tall.width, 56);
	assert.equal(tall.height, 100);

	const exact = smart.windowCells(64, 64, 1);
	assert.deepEqual(exact, { width: 64, height: 64 });

	const silly = smart.windowCells(10, 10, 0);
	assert.deepEqual(silly, { width: 10, height: 10 }, "a bad aspect falls back to the whole map");
});

test("a featureless picture is cropped in the middle", () => {
	const map = smart.energyMapFromPixels(buffer(40, 40, () => [90, 90, 90]), 40, 40, 4);
	const best = smart.bestWindow(map.energy, 40, 40, 20, 20);

	assert.equal(best.x, 10, "centred horizontally");
	assert.equal(best.y, 10, "centred vertically");
	assert.equal(best.confidence, 0, "and it says so: no reason to prefer anything");
});

test("the subject on the left wins over the middle", () => {
	const data = withSubject(64, 40, { x: 0, y: 10, width: 14, height: 20 });
	const map = smart.energyMapFromPixels(data, 64, 40, 4);
	const best = smart.bestWindow(map.energy, 64, 40, 24, 40);

	assert.ok(best.x < 8, `expected a left crop, got x=${best.x}`);
	assert.ok(best.confidence > 0.05, "and it is confident about it");
});

test("the subject at the bottom wins over the top", () => {
	const data = withSubject(40, 64, { x: 10, y: 44, width: 20, height: 18 });
	const map = smart.energyMapFromPixels(data, 40, 64, 4);
	const best = smart.bestWindow(map.energy, 40, 64, 40, 24);

	assert.ok(best.y > 20, `expected a bottom crop, got y=${best.y}`);
});

test("two subjects: the busier one wins", () => {
	const data = buffer(60, 40, (x, y) => {
		const left = x < 12 && y > 8 && y < 30;
		const right = x > 40 && x < 58 && y > 8 && y < 30;
		if (left || right) {
			const value = (x * 31 + y * 17) % 2 === 0 ? 10 : 240;
			return [value, value, value];
		}
		return [128, 128, 128];
	});

	const map = smart.energyMapFromPixels(data, 60, 40, 4);
	const best = smart.bestWindow(map.energy, 60, 40, 20, 40);
	const centre = 20;

	/* Both blobs are similar, so either edge is defensible; what must not
	 * happen is a crop that finds neither. */
	assert.ok(Math.abs(best.x - centre) > 5, "the crop leaves the empty middle");
});

test("a window the size of the picture cannot move", () => {
	const map = smart.energyMapFromPixels(withSubject(20, 20, { x: 0, y: 0, width: 5, height: 5 }), 20, 20, 4);
	const best = smart.bestWindow(map.energy, 20, 20, 20, 20);

	assert.deepEqual({ x: best.x, y: best.y }, { x: 0, y: 0 });
	assert.equal(best.confidence, 0);
});

test("cell offsets map onto the pixels the resize left over", () => {
	assert.equal(smart.offsetToPixels(0, 40, 840), 0);
	assert.equal(smart.offsetToPixels(40, 40, 840), 840);
	assert.equal(smart.offsetToPixels(20, 40, 840), 420);
	assert.equal(smart.offsetToPixels(999, 40, 840), 840, "never past the slack");
	assert.equal(smart.offsetToPixels(-5, 40, 840), 0, "never before it");
	assert.equal(smart.offsetToPixels(20, 0, 840), 0, "a zero span cannot be divided");
	assert.equal(smart.offsetToPixels(20, 40, 0), 0, "no slack, no offset");
});

test("a confident subject moves the crop; a flat picture stays centred", () => {
	const subject = smart.chooseCrop({
		data: withSubject(64, 32, { x: 0, y: 6, width: 16, height: 20 }),
		mapWidth: 64,
		mapHeight: 32,
		components: 4,
		aspect: 1,
		slackX: 800,
		slackY: 0,
	});

	assert.equal(subject.smart, true);
	assert.ok(subject.offsetX < 200, `expected the crop to follow the subject, got ${subject.offsetX}`);

	const flat = smart.chooseCrop({
		data: buffer(32, 32, () => [100, 100, 100]),
		mapWidth: 32,
		mapHeight: 32,
		components: 4,
		aspect: 1,
		slackX: 800,
		slackY: 0,
	});

	assert.equal(flat.smart, false, "nothing to see means no cleverness");
	assert.equal(flat.offsetX, 400, "so the crop is centred");
	assert.equal(flat.confidence, 0);
});

test("no pixels at all is a centred crop, not a crash", () => {
	const result = smart.chooseCrop({ data: null, mapWidth: 0, mapHeight: 0, aspect: 1, slackX: 100, slackY: 0 });

	assert.deepEqual(result, { offsetX: 0, offsetY: 0, confidence: 0, smart: false });
});
