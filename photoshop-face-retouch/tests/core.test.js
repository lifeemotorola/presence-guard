/*
Face Retouch for Photoshop — tests for the retouch arithmetic.
Run with: npm test   (node --test, no Photoshop needed)
*/

"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");

const core = require("../src/core.js");

const DOCUMENT = { width: 1800, height: 1800 };

function planFor(overrides) {
	return core.plan(Object.assign({ presetId: "natural", strength: 60, texture: 50 }, DOCUMENT, overrides || {}));
}

test("the presets are sane: unique ids, real numbers, valid colours", () => {
	assert.ok(core.PRESETS.length >= 4, "there should be a few looks to choose from");

	const ids = new Set();

	for (const preset of core.PRESETS) {
		assert.ok(!ids.has(preset.id), `duplicate preset id ${preset.id}`);
		ids.add(preset.id);

		assert.ok(preset.label && preset.blurb, `${preset.id} has a label and a description`);
		assert.ok(preset.smoothRadius > 0, `${preset.id} has a smoothing radius`);

		for (const key of ["colourOpacity", "liftOpacity", "glowOpacity", "depthOpacity", "warmthOpacity", "detailOpacity"]) {
			assert.ok(preset[key] >= 0 && preset[key] <= 100, `${preset.id}.${key} is a percentage`);
		}

		for (const channel of ["red", "green", "blue"]) {
			const value = preset.tint[channel];
			assert.ok(Number.isInteger(value) && value >= 0 && value <= 255, `${preset.id}.tint.${channel} is a colour`);
		}
	}

	assert.equal(core.getPreset("nope"), null);
	assert.equal(core.getPreset("glamour").id, "glamour");
});

test("the radii follow the document, not the screen the panel was opened on", () => {
	assert.equal(core.scaleFor(1800, 1800), 1);
	assert.ok(Math.abs(core.scaleFor(3600, 3600) - 2) < 1e-9);
	assert.equal(core.scaleFor(200, 200), 0.45, "tiny documents stop shrinking the blur");
	assert.equal(core.scaleFor(9000, 9000), 2.6, "and huge ones stop growing it");
	assert.equal(core.scaleFor(0, 0), 1, "a missing size is treated as reference size");
});

test("a 24 megapixel portrait gets a bigger blur than a 600px crop", () => {
	const big = core.smoothingRadius(core.getPreset("glamour"), 60, 6000, 4000);
	const small = core.smoothingRadius(core.getPreset("glamour"), 60, 600, 400);

	assert.ok(big > small, `${big} should exceed ${small}`);
	assert.ok(small >= core.RADIUS_MIN && big <= core.RADIUS_MAX);
});

test("the blur radius stays inside the limits even for absurd inputs", () => {
	const mad = { smoothRadius: 5000 };

	assert.equal(core.smoothingRadius(mad, 100, 1800, 1800), core.RADIUS_MAX);
	assert.equal(core.smoothingRadius({ smoothRadius: 0.01 }, 0, 200, 200), core.RADIUS_MIN);
	assert.equal(core.smoothingRadius(mad, NaN, 1800, 1800), core.smoothingRadius(mad, 0, 1800, 1800));
});

test("strength decides how far the sliders go, never whether they are on", () => {
	const whisper = core.plan({ presetId: "glamour", strength: 0, texture: 50, width: 1800, height: 1800 });
	const full = core.plan({ presetId: "glamour", strength: 100, texture: 50, width: 1800, height: 1800 });

	assert.ok(whisper.radius > 0, "there is still something at zero");
	assert.ok(full.radius > whisper.radius * 2, "and a lot more at one hundred");
});

test("the default plan is the six layers, top first", () => {
	const plan = planFor();

	assert.deepEqual(
		plan.steps.map((step) => step.id),
		["detail", "warmth", "depth", "glow", "lift", "smooth"],
		"top layer first — copies land directly above the original, so this is the order they are made in",
	);

	for (const step of plan.steps) {
		assert.ok(step.name.startsWith(core.PREFIX), `${step.name} is named so it can be found again`);
		assert.ok(step.opacity >= 1 && step.opacity <= 100, `${step.name} opacity is a percentage`);
		assert.ok(Number.isInteger(step.opacity), `${step.name} opacity is whole`);
		assert.ok(["overlay", "softLight", "screen", "lighten", "color"].includes(step.blend), `${step.name} blends`);
	}

	assert.equal(plan.steps[0].blend, "overlay", "texture goes on top");
	assert.equal(plan.steps[5].blend, "color", "tone evens out underneath");
});

test("the numbers for 1800px / Natural / 60 are the ones in the README", () => {
	const plan = planFor();
	const byId = Object.fromEntries(plan.steps.map((step) => [step.id, step]));

	assert.equal(plan.radius, 4);

	assert.equal(byId.smooth.filter.radius, 4);
	assert.equal(byId.smooth.opacity, 40);

	assert.equal(byId.lift.filter.radius, 8.8);
	assert.equal(byId.lift.opacity, 10);

	assert.equal(byId.glow.filter.radius, 13.6);
	assert.equal(byId.glow.opacity, 6);

	assert.equal(byId.depth.opacity, 12);
	assert.equal(byId.warmth.opacity, 6);
	assert.equal(byId.detail.filter.radius, 1.9);
	assert.equal(byId.detail.opacity, 23);
});

test("Glamour is stronger than Editorial everywhere it should be", () => {
	const glamour = Object.fromEntries(planFor({ presetId: "glamour" }).steps.map((step) => [step.id, step]));
	const editorial = Object.fromEntries(planFor({ presetId: "editorial" }).steps.map((step) => [step.id, step]));

	assert.ok(glamour.smooth.opacity > editorial.smooth.opacity);
	assert.ok(glamour.smooth.filter.radius > editorial.smooth.filter.radius);
	assert.ok(glamour.detail.opacity < editorial.detail.opacity, "editorial keeps more texture");
});

test("a switch really removes the layer", () => {
	const plan = planFor({ options: { warmth: false, glow: false } });
	const ids = plan.steps.map((step) => step.id);

	assert.ok(!ids.includes("warmth"));
	assert.ok(!ids.includes("glow"));
	assert.ok(ids.includes("smooth"), "the rest is untouched");
});

test("everything off is an empty plan with an explanation, not a crash", () => {
	const plan = planFor({
		options: { smooth: false, lift: false, glow: false, depth: false, warmth: false, detail: false },
	});

	assert.deepEqual(plan.steps, []);
	assert.match(plan.summary, /nothing to do/i);
});

test("texture at zero means no detail layer at all", () => {
	const plan = planFor({ texture: 0 });
	assert.ok(!plan.steps.some((step) => step.id === "detail"));

	const full = planFor({ texture: 100 });
	assert.equal(full.steps[0].id, "detail");
	assert.equal(full.steps[0].opacity, 45, "Natural restores texture at 45%");
});

test("the summary says what happened in words a person can read", () => {
	const plan = planFor({ presetId: "soft" });

	assert.match(plan.summary, /^Soft:/);
	assert.match(plan.summary, /6 layers/);
	assert.ok(plan.summary.includes("4px") === false, "the radius is whatever the maths said");
	assert.ok(plan.summary.includes(`${plan.radius}px`));
});

test("retouch layers are recognised by name, and nothing else is", () => {
	assert.equal(core.isRetouchLayer("FR · Smooth"), true);
	assert.equal(core.isRetouchLayer("Face Retouch"), true);
	assert.equal(core.isRetouchLayer("Background"), false);
	assert.equal(core.isRetouchLayer("FR · "), false, "the bare prefix is not a layer");
	assert.equal(core.isRetouchLayer(null), false);
	assert.equal(core.isRetouchLayer(42), false);

	assert.equal(core.hasRetouch(["Background", "FR · Glow"]), true);
	assert.equal(core.hasRetouch(["Background", "Curves 1"]), false);
	assert.equal(core.hasRetouch(null), false);
});

test("an unknown preset falls back instead of throwing", () => {
	const plan = core.plan({ presetId: "does-not-exist", width: 1800, height: 1800 });
	assert.equal(plan.preset.id, core.DEFAULTS.presetId);
});

test("the plan survives nonsense in the settings object", () => {
	const plan = core.plan({ presetId: "natural", strength: "abc", texture: null, width: undefined, height: -5 });

	assert.equal(plan.strength, core.DEFAULTS.strength);
	assert.equal(plan.texture, core.DEFAULTS.texture, "a missing texture falls back to the default, not to zero");
	assert.ok(plan.steps.length >= 1);
	assert.ok(plan.radius >= core.RADIUS_MIN);
});
