/* A simulated Photoshop 24/25 host. It deliberately makes activeLayers read-only,
 * as it was before Photoshop 26.9. Real Photoshop validation is still required. */
"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const path = require("node:path");
const { createHarness } = require("../../tests/uxp-harness.js");
const ROOT = path.resolve(__dirname, "..");

function fakePhotoshop(options = {}) {
	let id = 1;
	let active;
	let inModal = false;
	const context = { isCancelled: false };
	const calls = [];
	const filters = [];
	const all = [];
	const doc = { id: 701, name: "portrait.psd", width: 1800, height: 1200, layers: [], selection: {} };
	Object.defineProperty(doc, "activeLayers", {
		get: () => active ? [active] : [],
		set: () => { throw new Error("Photoshop 24/25 activeLayers is read-only"); },
	});
	function containerOf(layer) {
		for (const container of [doc, ...all.filter((item) => item.layers)]) {
			if (container.layers.includes(layer)) return container;
		}
		return null;
	}
	function newLayer(name, group = false) {
		const layer = {
			id: id++, name, visible: true, opacity: 100,
			layers: group ? [] : null,
			_blend: "normal",
			get blendMode() { return this._blend; },
			set blendMode(value) {
				if (options.failBlend && this.name === "FR · Warmth") throw new Error("blend refused");
				this._blend = value;
			},
			async duplicate() {
				assert.ok(inModal);
				const copy = newLayer(`${this.name} copy`);
				doc.layers.unshift(copy);
				active = copy;
				return copy;
			},
			async move(target, placement) {
				assert.ok(inModal);
				if (options.cancelOnMove && placement === "placeAfter") {
					context.isCancelled = true;
					throw new Error("cancelled while ordering");
				}
				if (options.failOrder && placement === "placeAfter") throw new Error("cannot reorder");
				if (options.failMove && placement === "placeInside") throw new Error("cannot group");
				const from = containerOf(this);
				if (from) from.layers.splice(from.layers.indexOf(this), 1);
				if (placement === "placeInside") target.layers.unshift(this);
				else if (placement === "placeAfter") {
					const parent = containerOf(target);
					if (!parent) throw new Error("target not found");
					parent.layers.splice(parent.layers.indexOf(target) + 1, 0, this);
				} else throw new Error(`bad placement: ${placement}`);
			},
			async delete() {
				assert.ok(inModal);
				if (options.failDelete || context.isCancelled) throw new Error("delete refused");
				const parent = containerOf(this);
				if (parent) parent.layers.splice(parent.layers.indexOf(this), 1);
				this.deleted = true;
			},
		};
		all.push(layer);
		return layer;
	}
	const base = newLayer("Portrait");
	doc.layers.push(base);
	active = base;
	const photoshop = {
		app: { activeDocument: doc },
		constants: {
			ElementPlacement: { PLACEINSIDE: "placeInside", PLACEAFTER: "placeAfter" },
			BlendMode: { OVERLAY: "host-overlay", SOFTLIGHT: "host-soft", SCREEN: "host-screen", LIGHTEN: "host-lighten", COLOR: "host-color" },
		},
		core: { async executeAsModal(fn) {
			inModal = true;
			try { return await fn(context); } finally { inModal = false; }
		} },
		action: { async batchPlay(descriptors) {
			assert.ok(inModal, "Photoshop changes must be modal");
			const descriptor = descriptors[0];
			calls.push(JSON.parse(JSON.stringify(descriptor)));
			if (descriptor._obj === "select") {
				assert.equal(descriptor._target[0]._ref, "layer");
				if (descriptor._target.length > 1) {
					assert.equal(descriptor._target[1]._ref, "document");
					assert.equal(descriptor._target[1]._id, doc.id);
					if (options.rejectDocumentReference)
						return [{ _obj: "error", message: "document reference unsupported", result: -44 }];
				}
				const target = all.find((layer) => layer.id === descriptor._target[0]._id && !layer.deleted);
				if (!target || options.failSelect) return [{ _obj: "error", message: "select refused", result: -123 }];
				active = target;
				return [{}];
			}
			if (descriptor._obj === "make") {
				if (descriptor._target[0]._ref === "layerSection") {
					const group = newLayer("Face Retouch", true);
					doc.layers.unshift(group);
					active = group;
				} else {
					const fill = newLayer("FR · Warmth");
					doc.layers.unshift(fill);
					active = fill;
				}
				return [{}];
			}
			if (["gaussianBlur", "highPass"].includes(descriptor._obj)) {
				filters.push({ name: active.name, type: descriptor._obj, visible: active.visible, opacity: active.opacity });
				if (!active.visible) return [{ _obj: "error", message: "filter target is hidden", result: -43 }];
				if (options.cancelOnFilter) {
					context.isCancelled = true;
					throw new Error("user cancelled");
				}
				if (options.failFilter === descriptor._obj)
					return [{ _obj: "error", message: "filter refused", result: -42 }];
				return [{}];
			}
			throw new Error(`unexpected action ${descriptor._obj}`);
		} },
	};
	return { photoshop, doc, base, all, calls, filters, context };
}

const settings = { presetId: "natural", strength: 60, texture: 50 };
function adapter(fake) { return createHarness(ROOT, { photoshop: fake.photoshop }).loadModule("./src/retouch.js"); }

test("retouch selects by layer/document ID on a read-only activeLayers host", async () => {
	const fake = fakePhotoshop();
	const result = await adapter(fake).apply(settings);
	assert.equal(result.steps.length, 6);
	assert.ok(result.steps.every((step) => step.ok));
	assert.equal(result.grouped, true);
	const group = fake.doc.layers.find((layer) => layer.name === "Face Retouch");
	assert.deepEqual(group.layers.map((layer) => layer.name), [
		"FR · Detail", "FR · Warmth", "FR · Depth", "FR · Glow", "FR · Lift", "FR · Smooth",
	]);
	assert.deepEqual(group.layers.map((layer) => layer.blendMode), [
		"host-overlay", "host-soft", "host-soft", "host-screen", "host-lighten", "host-color",
	]);
	assert.ok(fake.calls.some((call) => call._obj === "select"));
	assert.ok(fake.filters.every((call) => call.name.startsWith("FR · ") && call.visible === true && call.opacity === 0),
		"Photoshop filters need visible layers; 0% opacity keeps unfinished layers safe");
	assert.equal(fake.base.name, "Portrait");
	assert.equal(fake.base.visible, true);
});

test("selection falls back to layer ID if a host rejects the document reference", async () => {
	const fake = fakePhotoshop({ rejectDocumentReference: true });
	const result = await adapter(fake).apply(settings);
	assert.ok(result.steps.every((step) => step.ok));
	assert.ok(fake.calls.some((call) => call._obj === "select" && call._target.length === 2));
	assert.ok(fake.calls.some((call) => call._obj === "select" && call._target.length === 1));
});

test("a returned Photoshop error is reported and the incomplete layer is removed", async () => {
	const fake = fakePhotoshop({ failFilter: "highPass" });
	const result = await adapter(fake).apply(settings);
	const detail = result.steps.find((step) => step.id === "detail");
	assert.equal(detail.ok, false);
	assert.match(detail.message, /filter refused/);
	const copy = fake.all.find((layer) => layer.name === "FR · Detail");
	assert.ok(copy.deleted || copy.visible === false);
	assert.equal(result.steps.filter((step) => step.ok).length, 5);
});

test("a failed fill never leaves a visible full-opacity tint if deletion also fails", async () => {
	const fake = fakePhotoshop({ failBlend: true, failDelete: true });
	const result = await adapter(fake).apply(settings);
	const warmth = result.steps.find((step) => step.id === "warmth");
	assert.equal(warmth.ok, false);
	assert.match(warmth.message, /blend refused/);
	const layer = fake.all.find((item) => item.name === "FR · Warmth");
	assert.equal(layer.visible, false);
	assert.match(warmth.message, /hidden|remove/i);
});

test("a refused selection never applies a filter to the wrong layer", async () => {
	const fake = fakePhotoshop({ failSelect: true });
	const result = await adapter(fake).apply(settings);
	assert.ok(result.steps.every((step) => !step.ok));
	assert.equal(fake.filters.length, 0);
	assert.equal(fake.doc.layers.length, 1);
	assert.match(result.steps[0].message, /select refused/);
});

test("grouping failures are not reported as a fully grouped retouch", async () => {
	const fake = fakePhotoshop({ failMove: true });
	const result = await adapter(fake).apply(settings);
	assert.equal(result.steps.filter((step) => step.ok).length, 6);
	assert.equal(result.grouped, false);
});

test("a refused reorder is surfaced instead of claiming an ordered stack", async () => {
	const fake = fakePhotoshop({ failOrder: true });
	const result = await adapter(fake).apply(settings);
	assert.equal(result.grouped, true);
	assert.match(result.warning, /could not arrange/i);
});

test("cancellation while arranging stops before grouping", async () => {
	const fake = fakePhotoshop({ cancelOnMove: true });
	const result = await adapter(fake).apply(settings);
	assert.equal(result.cancelled, true);
	assert.equal(result.grouped, false);
	assert.ok(!fake.calls.some((call) => call._obj === "make" && call._target[0]._ref === "layerSection"));
});

test("cancellation stops further steps and leaves an unfinished copy hidden", async () => {
	const fake = fakePhotoshop({ cancelOnFilter: true });
	const result = await adapter(fake).apply(settings);
	assert.equal(result.cancelled, true);
	assert.equal(result.steps.length, 1);
	assert.equal(fake.filters.length, 1);
	const copy = fake.all.find((layer) => layer.name === "FR · Detail");
	assert.ok(copy.visible === false || copy.opacity === 0, "unfinished copy has no visible effect");
});

test("remove requires an explicit confirmation in the same document", async () => {
	const fake = fakePhotoshop();
	const host = createHarness(ROOT, { photoshop: fake.photoshop });
	await host.loadModule("./src/retouch.js").apply(settings);
	const panel = host.loadHTML().panels.faceRetouchPanel;
	panel.create(host.document.body);
	panel.show(host.document.body);
	const remove = host.document.getElementById("remove");
	await remove.dispatch("click");
	assert.equal(remove.textContent, "Confirm remove");
	assert.match(host.document.getElementById("status").textContent, /including any you made/);
	assert.ok(fake.doc.layers.some((layer) => layer.name === "Face Retouch"), "first click removes nothing");
	await remove.dispatch("click");
	assert.ok(!fake.doc.layers.some((layer) => layer.name === "Face Retouch"));
	assert.ok(fake.doc.layers.includes(fake.base));
});

test("remove refuses if the active document changed after confirmation", async () => {
	const fake = fakePhotoshop();
	const kit = adapter(fake);
	await kit.apply(settings);
	await assert.rejects(() => kit.removeAll(fake.doc.id + 1), /active document changed/i);
	assert.ok(fake.doc.layers.some((layer) => layer.name === "Face Retouch"));
});


test("remove reports refused deletions rather than claiming there were no matches", async () => {
	const fake = fakePhotoshop({ failDelete: true });
	const kit = adapter(fake);
	assert.equal((await kit.apply(settings)).grouped, true);
	const result = await kit.removeAll(fake.doc.id);
	assert.equal(result.removed, 0);
	assert.equal(result.failed, 1);
	assert.ok(fake.doc.layers.some((layer) => layer.name === "Face Retouch"));
});

test("remove deletes a matching group once, rather than reporting its children as failures", async () => {
	const fake = fakePhotoshop();
	const kit = adapter(fake);
	assert.equal((await kit.apply(settings)).grouped, true);
	const result = await kit.removeAll(fake.doc.id);
	assert.equal(result.removed, 1);
	assert.equal(result.failed, 0);
	assert.ok(!fake.doc.layers.some((layer) => layer.name === "Face Retouch"));
});
