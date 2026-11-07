/*
Face Retouch for Photoshop — tests for the three descriptors that have to be
written by hand. Everything a test can reach without Photoshop lives here.
*/

"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");

const core = require("../src/core.js");
const descriptors = require("../src/descriptors.js");

test("a blur says what it is, on which layer, in which units", () => {
	const descriptor = descriptors.gaussianBlur(4.2);

	assert.equal(descriptor._obj, "gaussianBlur");
	assert.deepEqual(descriptor._target, [{ _ref: "layer", _enum: "ordinal", _value: "targetEnum" }]);
	assert.deepEqual(descriptor.radius, { _unit: "pixelsUnit", _value: 4.2 });
	assert.equal(descriptor._options.dialogOptions, "dontDisplay", "the filter must not open a dialog");
});

test("a high pass is the same shape with a different name", () => {
	const descriptor = descriptors.highPass(1.9);

	assert.equal(descriptor._obj, "highPass");
	assert.equal(descriptor.radius._value, 1.9);
	assert.equal(descriptor._options.dialogOptions, "dontDisplay");
});

test("radii are clamped to something Photoshop will accept", () => {
	assert.equal(descriptors.gaussianBlur(0).radius._value, core.RADIUS_MIN);
	assert.equal(descriptors.gaussianBlur(-12).radius._value, core.RADIUS_MIN);
	assert.equal(descriptors.gaussianBlur(9999).radius._value, core.RADIUS_MAX);
	assert.equal(descriptors.gaussianBlur("nonsense").radius._value, core.RADIUS_MIN);
	assert.equal(descriptors.gaussianBlur(2.34567).radius._value, 2.35, "two decimals is plenty");
});

test("a solid colour layer carries the tint in the channels Photoshop reads", () => {
	const descriptor = descriptors.solidColour({ red: 255, green: 216, blue: 186 }, "FR · Warmth");

	assert.equal(descriptor._obj, "make");
	assert.deepEqual(descriptor._target, [{ _ref: "contentLayer" }]);
	assert.equal(descriptor.using.name, "FR · Warmth");

	const colour = descriptor.using.type.color;
	assert.equal(descriptor.using.type._obj, "solidColorLayer");
	assert.equal(colour._obj, "RGBColor");
	assert.equal(colour.red, 255);
	assert.equal(colour.grain, 216, "green is called grain in this descriptor, and always has been");
	assert.equal(colour.blue, 186);
});

test("colour channels are clamped to 0-255 rather than passed through", () => {
	const colour = descriptors.solidColour({ red: 400, green: -20, blue: 12.7 }, "x").using.type.color;

	assert.equal(colour.red, 255);
	assert.equal(colour.grain, 0);
	assert.equal(colour.blue, 13);
});

test("a group is a layer section with a name", () => {
	const descriptor = descriptors.layerGroup("Face Retouch");

	assert.equal(descriptor._obj, "make");
	assert.deepEqual(descriptor._target, [{ _ref: "layerSection" }]);
	assert.equal(descriptor.using.name, "Face Retouch");
});

test("every step of a real plan maps to the descriptor it needs", () => {
	const plan = core.plan({ presetId: "natural", strength: 60, texture: 50, width: 1800, height: 1800 });

	for (const step of plan.steps) {
		const descriptor = descriptors.forStep(step);

		if (step.filter.type === "none") {
			assert.equal(descriptor, null, `${step.name} is a plain copy — no filter, no descriptor`);
			continue;
		}

		assert.ok(descriptor, `${step.name} has a descriptor`);
		assert.equal(typeof descriptor._obj, "string");
	}

	const smooth = plan.steps.find((step) => step.id === "smooth");
	assert.equal(descriptors.forStep(smooth)._obj, "gaussianBlur");

	const detail = plan.steps.find((step) => step.id === "detail");
	assert.equal(descriptors.forStep(detail)._obj, "highPass");

	const warmth = plan.steps.find((step) => step.id === "warmth");
	assert.equal(descriptors.forStep(warmth)._obj, "make");
});

test("everything sent to batchPlay survives JSON", () => {
	/* batchPlay takes a plain object; an undefined or a function in there is a
	 * silent failure at run time and a confusing one. */
	const payloads = [
		descriptors.gaussianBlur(4),
		descriptors.highPass(2),
		descriptors.solidColour({ red: 255, green: 210, blue: 180 }, "FR · Warmth"),
		descriptors.layerGroup(core.GROUP_NAME),
	];

	for (const payload of payloads) {
		const round = JSON.parse(JSON.stringify(payload));
		assert.deepEqual(round, payload, `${payload._obj} is plain data`);
	}
});
