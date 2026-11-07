/*
Face Retouch for Photoshop — checks the things Photoshop checks, but in a place
where the answer is a line number instead of "the plugin could not be loaded".
*/

"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const ROOT = path.join(__dirname, "..");
const read = (file) => fs.readFileSync(path.join(ROOT, file), "utf8");
const exists = (file) => fs.existsSync(path.join(ROOT, file));

const manifest = JSON.parse(read("manifest.json"));

/** Width and height straight out of the PNG header — what Photoshop reads. */
function pngSize(file) {
	const buffer = fs.readFileSync(path.join(ROOT, file));

	assert.equal(buffer.toString("ascii", 1, 4), "PNG", `${file} is a PNG`);

	return { width: buffer.readUInt32BE(16), height: buffer.readUInt32BE(20) };
}

test("the manifest is a loadable manifest version 5", () => {
	assert.equal(manifest.manifestVersion, 5);
	assert.match(manifest.id, /^com\.presenceguard\.[a-z]+$/, "the id is namespaced and lower case");
	assert.equal(manifest.name, "Face Retouch");
	assert.match(manifest.version, /^\d+\.\d+\.\d+$/, "the store wants a three-part version");
	assert.equal(manifest.main, "index.html");
	assert.ok(exists(manifest.main), "main points at a file that is there");
});

test("it declares Photoshop 2023 or newer, and nothing older", () => {
	const hosts = Array.isArray(manifest.host) ? manifest.host : [manifest.host];
	const photoshop = hosts.find((host) => host.app === "PS");

	assert.ok(photoshop, "Photoshop is the host");
	assert.equal(photoshop.minVersion, "24.0.0", "24.0 is Photoshop 2023, the version the panel is written against");
	assert.equal(photoshop.maxVersion, undefined, "no upper limit — newer versions keep working");
});

test("the entry points the panel registers are the ones the manifest declares", () => {
	const entrypoints = manifest.entrypoints || [];
	const panel = entrypoints.find((entry) => entry.type === "panel");
	const command = entrypoints.find((entry) => entry.type === "command");

	assert.ok(panel, "there is a panel");
	assert.ok(command, "and a menu command");

	for (const entry of entrypoints) {
		assert.ok(entry.id, `${entry.type} has an id`);
		assert.ok(entry.label && entry.label.default, `${entry.id} has a label`);

		/* entrypoints.setup({ panels: { id: … }, commands: { id: … } }) */
		assert.match(read("src/panel.js"), new RegExp(entry.id + "\\s*:"), `panel.js registers ${entry.id}`);
	}

	assert.ok(panel.minimumSize && panel.minimumSize.width >= 280, "the panel has a sane minimum width");
});

test("the icons are the sizes the manifest claims", () => {
	assert.ok((manifest.icons || []).length > 0, "there is an icon");

	for (const icon of manifest.icons) {
		assert.ok(exists(icon.path), `icon missing: ${icon.path}`);

		const single = pngSize(icon.path);
		assert.deepEqual(single, { width: icon.width, height: icon.height }, `${icon.path} is ${icon.width}×${icon.height}`);

		if ((icon.scale || []).includes(2)) {
			const double = icon.path.replace(/\.png$/, "@2x.png");
			assert.ok(exists(double), `the 2x icon is missing: ${double}`);
			assert.deepEqual(pngSize(double), { width: icon.width * 2, height: icon.height * 2 }, `${double} is the double`);
		}

		assert.ok((icon.theme || []).length >= 2, "the icon is offered for light and dark host themes");
	}
});

test("every file the panel loads is in the folder", () => {
	for (const file of ["index.html", "styles.css", "src/panel.js", "src/core.js", "src/descriptors.js", "src/retouch.js", "README.md"]) {
		assert.ok(exists(file), `missing: ${file}`);
	}

	const html = read("index.html");
	const referenced = [...html.matchAll(/(?:href|src)="([^"]+)"/g)].map((match) => match[1]);

	assert.ok(referenced.length >= 2, "the page pulls in a stylesheet and the panel script");

	for (const file of referenced) {
		assert.ok(!/^https?:/i.test(file), `${file} is local — a plugin has no network permission`);
		assert.ok(exists(file), `the page references a missing file: ${file}`);
	}
});

test("every element the panel looks up is in the page", () => {
	const html = read("index.html");
	const ids = new Set([...html.matchAll(/id="([^"]+)"/g)].map((match) => match[1]));
	const looked = [...read("src/panel.js").matchAll(/el\("([^"]+)"\)/g)].map((match) => match[1]);

	assert.ok(looked.length > 8, "the panel does look things up");

	/* A typo here is a panel that opens blank, with nothing in the console. */
	for (const id of new Set(looked))
		assert.ok(ids.has(id), `index.html has no #${id}`);
});

test("the plugin never asks for a permission it does not use", () => {
	for (const permission of Object.keys(manifest.requiredPermissions || {}))
		assert.equal(permission, "localFileSystem", `${permission} has no business here`);

	/* Nothing in this plugin touches the network or the file system: it is all
	 * layers inside the open document. */
	for (const file of ["src/panel.js", "src/retouch.js", "src/core.js", "src/descriptors.js"]) {
		const source = read(file);
		assert.ok(!/fetch\s*\(/.test(source), `${file} makes no network call`);
		assert.ok(!/XMLHttpRequest/.test(source), `${file} makes no network call`);
	}
});

test("every script parses, as the kind of module UXP loads", () => {
	for (const file of ["src/panel.js", "src/retouch.js", "src/core.js", "src/descriptors.js"]) {
		const source = read(file);

		assert.doesNotThrow(() => new vm.Script(source, { filename: file }), `${file} parses`);

		/* UXP loads these with require(), so no top-level import/export. */
		assert.ok(!/^\s*(import|export)\s/m.test(source), `${file} is CommonJS`);
	}
});
