/*
Export Kit for Photoshop — tests for the export arithmetic.
Run with: npm test  (node --test)
*/

"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");

const core = require("../src/core.js");

test("the slot list is sane: unique ids, real sizes, known modes and formats", () => {
	assert.ok(core.SLOTS.length >= 8, "there should be a decent set of slots built in");

	const ids = new Set();
	for (const slot of core.SLOTS) {
		assert.ok(!ids.has(slot.id), `duplicate slot id ${slot.id}`);
		ids.add(slot.id);

		assert.ok(slot.width > 0 && slot.height > 0, `${slot.id} has a size`);
		assert.ok(Number.isInteger(slot.width) && Number.isInteger(slot.height), `${slot.id} uses whole pixels`);
		assert.ok(slot.mode === "fill" || slot.mode === "fit", `${slot.id} has a mode`);
		assert.ok(slot.format === "jpeg" || slot.format === "png", `${slot.id} has a format`);
		assert.ok(slot.label && slot.group, `${slot.id} has a label and a group`);
	}

	assert.deepEqual(core.defaultSelection(), ["yt-thumb", "ig-post", "ig-story", "x-post"]);
	for (const id of core.defaultSelection()) assert.ok(core.getSlot(id), `${id} is a real slot`);

	assert.equal(core.getSlot("nope"), null);
});

test("a 16:9 document fills a 16:9 slot exactly, with no slack and no crop", () => {
	const job = core.planJob(core.getSlot("yt-thumb"), 1920, 1080);

	assert.equal(job.mode, "fill");
	assert.deepEqual(job.resize, { width: 1280, height: 720 });
	assert.equal(job.slackX, 0);
	assert.equal(job.slackY, 0);
	assert.deepEqual(job.crop, { left: 0, top: 0, right: 1280, bottom: 720 });
	assert.equal(job.upscale, false);
	assert.equal(job.ext, "jpg");
});

test("a 4:3 document is cropped top and bottom, not squashed", () => {
	const job = core.planJob(core.getSlot("yt-thumb"), 1600, 1200);

	assert.deepEqual(job.resize, { width: 1280, height: 960 });
	assert.equal(job.slackX, 0, "no sideways freedom");
	assert.equal(job.slackY, 240, "240 pixels of vertical freedom");
	/* Centred to begin with: the smart crop moves it later. */
	assert.deepEqual(job.crop, { left: 0, top: 120, right: 1280, bottom: 840 });
});

test("a square document becomes a vertical story by cropping the sides", () => {
	const job = core.planJob(core.getSlot("ig-story"), 1080, 1080);

	assert.deepEqual(job.resize, { width: 1920, height: 1920 });
	assert.deepEqual(job.target, { width: 1080, height: 1920 });
	assert.equal(job.slackX, 840);
	assert.equal(job.slackY, 0);
	assert.deepEqual(job.crop, { left: 420, top: 0, right: 1500, bottom: 1920 });
});

test("fit keeps everything and pads instead of cropping", () => {
	const job = core.planJob(core.getSlot("yt-endcard"), 1080, 1080);

	assert.equal(job.mode, "fit");
	assert.deepEqual(job.resize, { width: 1080, height: 1080 });
	assert.deepEqual(job.target, { width: 1920, height: 1080 });
	assert.equal(job.crop, null, "nothing is cropped in fit");
	assert.equal(job.anchor, "MIDDLECENTER");
});

test("rounding never leaves the cover a pixel short", () => {
	/* 1000x563 scaled to cover 1280x720 is 1280x720.64: floor would leave a
	 * transparent seam, so the resize is rounded up to cover. */
	const job = core.planJob(core.getSlot("yt-thumb"), 1000, 563);

	assert.ok(job.resize.width >= job.target.width, "covers horizontally");
	assert.ok(job.resize.height >= job.target.height, "covers vertically");
	assert.ok(job.crop.right - job.crop.left === job.target.width);
	assert.ok(job.crop.bottom - job.crop.top === job.target.height);
	assert.ok(job.crop.right <= job.resize.width && job.crop.bottom <= job.resize.height);
});

test("a small document is reported as a blow-up", () => {
	const job = core.planJob(core.getSlot("yt-thumb"), 400, 300);

	assert.equal(job.upscale, true);
	assert.ok(job.upscaleFactor > 3);
	assert.equal(job.warnings.length, 1);
	assert.match(job.warnings[0], /blow-up/);

	const mild = core.planJob(core.getSlot("yt-thumb"), 1200, 700);
	assert.equal(mild.upscale, true);
	assert.equal(mild.warnings.length, 0, "a mild enlargement is not worth a warning");
});

test("sharpening an enlarged image is called out", () => {
	const job = core.planJob(core.getSlot("ig-post"), 500, 500, { sharpen: true });
	assert.equal(job.upscale, true);
	assert.ok(job.warnings.some((warning) => /sharpen/i.test(warning)));
});

test("the crop can be moved inside the slack, never outside it", () => {
	const job = core.planJob(core.getSlot("yt-thumb"), 1600, 1200);

	const moved = core.withCropOffset(job, 0, 0);
	assert.deepEqual(moved.crop, { left: 0, top: 0, right: 1280, bottom: 720 });

	const toBottom = core.withCropOffset(job, 0, 9999);
	assert.deepEqual(toBottom.crop, { left: 0, top: 240, right: 1280, bottom: 960 });

	const negative = core.withCropOffset(job, -50, -50);
	assert.deepEqual(negative.crop, { left: 0, top: 0, right: 1280, bottom: 720 });

	assert.equal(core.withCropOffset(job, 10, 10).crop.right - core.withCropOffset(job, 10, 10).crop.left, 1280);
});

test("a job with nothing to crop is left alone", () => {
	const fit = core.planJob(core.getSlot("yt-endcard"), 1080, 1080);
	assert.equal(core.withCropOffset(fit, 100, 100), fit);
});

test("file names are predictable and safe on every filesystem", () => {
	assert.equal(core.sanitiseBaseName("Recording 01.PSD"), "recording-01");
	assert.equal(core.sanitiseBaseName("My Stream - FINAL final"), "my-stream-final-final");
	assert.equal(core.sanitiseBaseName("Café Déjà vu"), "cafe-deja-vu");
	assert.equal(core.sanitiseBaseName(""), "design");
	assert.equal(core.sanitiseBaseName(null), "design");
	assert.equal(core.sanitiseBaseName("!!!"), "design");
	assert.equal(core.sanitiseBaseName("___"), "design");
	assert.equal(core.sanitiseBaseName("a--b"), "a-b");
	assert.ok(core.sanitiseBaseName("x".repeat(200)).length <= 60);

	assert.equal(core.fileName("My Design.psd", core.getSlot("yt-thumb")), "my-design-yt-thumb-1280x720.jpg");
	assert.equal(
		core.fileName("My Design.psd", core.getSlot("discord-emoji")),
		"my-design-discord-emoji-128x128.png",
		"png slots keep their extension",
	);
});

test("sizes read like sizes", () => {
	assert.equal(core.formatBytes(512), "512 B");
	assert.equal(core.formatBytes(2048), "2 KB");
	assert.equal(core.formatBytes(5 * 1024 * 1024), "5.0 MB");
	assert.equal(core.fileNameForJob("My Design.psd", core.planJob(core.getSlot("ig-post"), 1000, 1000)),
		"my-design-ig-post-1080x1080.jpg", "a job knows its own file name");
	assert.equal(core.formatBytes(NaN), "");
	assert.equal(core.formatBytes(-1), "");
});

test("an export plan follows the slot order and ignores what is unticked", () => {
	const plan = core.planExport(1920, 1080, ["ig-story", "yt-thumb"]);

	assert.equal(plan.length, 2);
	assert.deepEqual(
		plan.map((job) => job.slotId),
		["yt-thumb", "ig-story"],
		"the plan is in slot order, not the order the user ticked them",
	);

	assert.deepEqual(core.planExport(1920, 1080, []), []);
	assert.deepEqual(core.planExport(1920, 1080, ["nonsense"]), []);
	assert.equal(core.planExport(1920, 1080, core.defaultSelection()).length, 4);
});

test("a document with no size is refused instead of guessed at", () => {
	assert.throws(() => core.planJob(core.getSlot("yt-thumb"), 0, 1080), /no size/);
	assert.throws(() => core.planJob(core.getSlot("yt-thumb"), 1920, 0), /no size/);
	assert.throws(() => core.planJob(core.getSlot("yt-thumb"), NaN, 1080), /no size/);
	assert.throws(() => core.planJob(null, 1920, 1080), /unknown slot/);
});

test("the summary counts what worked, what did not, and how big it all is", () => {
	const all = core.summarise([
		{ ok: true, bytes: 2048 },
		{ ok: true, bytes: 1024 },
	]);
	assert.equal(all.ok, 2);
	assert.equal(all.failed, 0);
	assert.equal(all.text, "2 files, 3 KB");

	const one = core.summarise([{ ok: true, bytes: 900 }]);
	assert.equal(one.text, "1 file, 900 B");

	const some = core.summarise([
		{ ok: true, bytes: 900 },
		{ ok: false, error: "denied" },
	]);
	assert.equal(some.text, "1 exported, 1 failed");

	const none = core.summarise([]);
	assert.equal(none.total, 0);
	assert.equal(none.text, "0 files");

	const unknown = core.summarise([{ ok: true, bytes: null }]);
	assert.equal(unknown.text, "1 file", "a size we could not read is not printed as 0 B");
});
