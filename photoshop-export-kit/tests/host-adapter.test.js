/* Simulates Photoshop modal, document, cancellation, and batchPlay behavior.
 * It does not execute the plugin inside Photoshop. */
"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const path = require("node:path");
const { createHarness } = require("../../tests/uxp-harness.js");
const logic = require("../src/core.js");
const ROOT = path.resolve(__dirname, "..");

function fakePhotoshop(options = {}) {
	let inModal = false;
	let nextId = 200;
	const copies = [];
	const autoClose = new Set();
	const events = [];
	const saved = [];
	const files = [];
	const context = {
		isCancelled: Boolean(options.cancelInitially),
		hostControl: {
			async registerAutoCloseDocument(id) { events.push(["register", id]); autoClose.add(id); },
			async unregisterAutoCloseDocument(id) { events.push(["unregister", id]); autoClose.delete(id); },
		},
	};
	const app = {};
	const doc = {
		id: 111, name: "Original.psd", width: 1920, height: 1080,
		async duplicate() {
			assert.ok(inModal);
			const copy = {
				id: nextId++, closed: false,
				async resizeImage(width, height) {
					events.push(["resize", this.id, width, height]);
					if (options.cancelOnResize) { context.isCancelled = true; throw new Error("cancelled"); }
					if (options.failFirstResize && copies.length === 1) throw new Error("resize refused");
				},
				async crop(bounds) { events.push(["crop", this.id, bounds]); },
				async resizeCanvas(width, height, anchor) { events.push(["canvas", this.id, width, height, anchor]); },
				saveAs: {
					async png(file) { saved.push(file.name); },
					async jpg(file) { saved.push(file.name); },
				},
				async closeWithoutSaving() {
					events.push(["close", this.id]);
					if (context.isCancelled) throw new Error("changes blocked after cancellation");
					this.closed = true;
					app.activeDocument = doc;
				},
			};
			copies.push(copy);
			app.activeDocument = copy;
			return copy;
		},
	};
	app.activeDocument = doc;
	const photoshop = {
		app,
		constants: { AnchorPosition: { MIDDLECENTER: "center" } },
		core: { async executeAsModal(fn) {
			inModal = true;
			try { return await fn(context); }
			finally {
				inModal = false;
				for (const copy of copies) {
					if (autoClose.has(copy.id) && !copy.closed) {
						copy.closed = true;
						events.push(["auto-close", copy.id]);
					}
				}
				app.activeDocument = doc;
			}
		} },
		action: { async batchPlay() {
			return options.failSharpen ? [{ _obj: "error", message: "filter refused", result: -42 }] : [{}];
		} },
		imaging: options.cancelOnImaging ? { async getPixels() {
			context.isCancelled = true;
			throw new Error("cancelled during analysis");
		} } : null,
	};
	const folder = { async createFile(name, opts) {
		files.push({ name, opts });
		return { name, size: 4096 };
	} };
	return { photoshop, doc, context, events, copies, saved, files, folder };
}

const settings = { quality: 10, sharpen: false, smartCrop: false };
const jobs = logic.planExport(1920, 1080, ["yt-thumb", "twitch-panel"], settings);
function adapter(fake) { return createHarness(ROOT, { photoshop: fake.photoshop }).loadModule("./src/photoshop.js"); }
function options(fake, overrides = {}) {
	return { folder: fake.folder, settings, baseName: fake.doc.name, documentId: fake.doc.id, ...overrides };
}

test("export duplicates, saves, and closes each copy without modifying the original", async () => {
	const fake = fakePhotoshop();
	const outcome = await adapter(fake).exportJobs(jobs, options(fake));
	assert.equal(outcome.cancelled, false);
	assert.equal(outcome.results.length, 2);
	assert.ok(outcome.results.every((result) => result.ok));
	assert.equal(fake.saved.length, 2);
	assert.ok(fake.copies.every((copy) => copy.closed));
	assert.equal(fake.files[0].opts.overwrite, true);
	assert.equal(fake.doc.name, "Original.psd");
	assert.equal(fake.doc.width, 1920);
	assert.ok(fake.events.some((event) => event[0] === "register"));
});

test("a boolean cancellation before the first job creates no copies", async () => {
	const fake = fakePhotoshop({ cancelInitially: true });
	const outcome = await adapter(fake).exportJobs(jobs, options(fake));
	assert.equal(outcome.cancelled, true);
	assert.equal(outcome.results.length, 0);
	assert.equal(fake.copies.length, 0);
});

test("cancellation during a job stops the batch and auto-closes its temporary document", async () => {
	const fake = fakePhotoshop({ cancelOnResize: true });
	const outcome = await adapter(fake).exportJobs(jobs, options(fake));
	assert.equal(outcome.cancelled, true);
	assert.equal(fake.copies.length, 1);
	assert.ok(fake.copies[0].closed);
	assert.ok(fake.events.some((event) => event[0] === "auto-close"));
	assert.equal(fake.saved.length, 0);
});

test("a non-cancellation error is reported and the next job is attempted", async () => {
	const fake = fakePhotoshop({ failFirstResize: true });
	const outcome = await adapter(fake).exportJobs(jobs, options(fake));
	assert.equal(outcome.cancelled, false);
	assert.equal(outcome.results.length, 2);
	assert.equal(outcome.results[0].ok, false);
	assert.match(outcome.results[0].error, /resize refused/);
	assert.equal(outcome.results[1].ok, true);
	assert.ok(fake.copies.every((copy) => copy.closed));
});

test("a returned batchPlay error means sharpening was skipped, not successful", async () => {
	const fake = fakePhotoshop({ failSharpen: true });
	const outcome = await adapter(fake).exportJobs(jobs.slice(0, 1), options(fake, {
		settings: { ...settings, sharpen: true },
	}));
	assert.equal(outcome.results[0].ok, true);
	assert.equal(outcome.results[0].sharpened, false);
});

test("cancellation during smart-crop analysis is not swallowed", async () => {
	const fake = fakePhotoshop({ cancelOnImaging: true });
	const smartSettings = { ...settings, smartCrop: true };
	const smartJobs = logic.planExport(1920, 1080, ["ig-story"], smartSettings);
	const outcome = await adapter(fake).exportJobs(smartJobs, options(fake, { settings: smartSettings }));
	assert.equal(outcome.cancelled, true);
	assert.equal(fake.copies.length, 0);
});

test("the export refuses to operate on a different active document", async () => {
	const fake = fakePhotoshop();
	await assert.rejects(() => adapter(fake).exportJobs(jobs, options(fake, { documentId: 999 })), /document changed/i);
	assert.equal(fake.copies.length, 0);
});

test("folder permissions are saved as a resolved persistent token", async () => {
	const fake = fakePhotoshop();
	const folder = { name: "Exports" };
	let received;
	const host = createHarness(ROOT, {
		photoshop: fake.photoshop,
		localFileSystem: {
			getFolder: async () => folder,
			createPersistentToken: async () => "persistent-token-123",
			getEntryForPersistentToken: async (token) => { received = token; return folder; },
		},
	});
	const kit = host.loadModule("./src/photoshop.js");
	assert.equal(await kit.chooseFolder(), folder);
	assert.equal(await kit.rememberedFolder(), folder);
	assert.equal(received, "persistent-token-123");
});

test("repeated export clicks cannot open multiple folder pickers", async () => {
	const fake = fakePhotoshop();
	let resolveFolder;
	let selections = 0;
	const waitForSelection = new Promise((resolve) => { resolveFolder = resolve; });
	const host = createHarness(ROOT, {
		photoshop: fake.photoshop,
		localFileSystem: { getFolder: () => { selections++; return waitForSelection; } },
	});
	const panel = host.loadHTML().panels.exportKitPanel;
	panel.create(host.document.body);
	panel.show(host.document.body);
	const button = host.document.getElementById("export");
	const first = button.dispatch("click");
	const second = button.dispatch("click");
	await new Promise((resolve) => setImmediate(resolve));
	assert.equal(selections, 1);
	resolveFolder(null);
	await Promise.all([first, second]);
});

test("unavailable smart-crop imaging is reported as a centered crop", async () => {
	const fake = fakePhotoshop();
	const smartSettings = { ...settings, smartCrop: true };
	const smartJobs = logic.planExport(1920, 1080, ["ig-story"], smartSettings);
	const outcome = await adapter(fake).exportJobs(smartJobs, options(fake, { settings: smartSettings }));
	assert.equal(outcome.results[0].ok, true);
	assert.equal(outcome.results[0].smart, false);
	assert.equal(outcome.results[0].cropDecision, "centered crop");
});
