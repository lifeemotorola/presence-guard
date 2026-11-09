/* These tests simulate UXP's HTML-entry resolution and v5 panel lifecycle.
 * They cannot replace a test inside Photoshop. */
"use strict";
const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const { createHarness, Element } = require("../../tests/uxp-harness.js");
const ROOT = process.env.UXP_TEST_PLUGIN_ROOT || path.resolve(__dirname, "..");

test("UXP-style HTML startup loads every dependency and registers manifest entrypoints", () => {
	const host = createHarness(ROOT);
	const setup = host.loadHTML();
	assert.equal(host.setups.length, 1);
	for (const entry of host.manifest.entrypoints) {
		const group = entry.type === "panel" ? setup.panels : setup.commands;
		const handler = group[entry.id];
		assert.ok(handler, `${entry.id} was registered`);
		if (entry.type === "command")
			assert.ok(typeof handler === "function" || typeof handler.run === "function", "command can be invoked");
	}
	for (const icon of host.manifest.icons || []) {
		assert.ok(fs.existsSync(path.join(ROOT, icon.path)), `${icon.path} is packaged`);
		if ((icon.scale || []).includes(2))
			assert.ok(fs.existsSync(path.join(ROOT, icon.path.replace(/\.png$/, "@2x.png"))), "2x icon is packaged");
	}
	for (const match of host.html.matchAll(/(?:href|src)="([^"]+)"/g))
		assert.ok(fs.existsSync(path.join(ROOT, match[1])), `HTML resource ${match[1]} is packaged`);
	assert.equal(host.manifest.host[0].data.apiVersion, 2);
	assert.equal(host.manifest.requiredPermissions.localFileSystem, "request");
	for (const file of ["index.js", "styles.css", "INSTALL.txt"]) {
		assert.ok(fs.existsSync(path.join(ROOT, file)), `${file} is packaged`);
	}
});

test("the v5 panel mounts, initializes once, and cleans up its refresh timer", () => {
	const host = createHarness(ROOT);
	const panel = host.loadHTML().panels.exportKitPanel;
	const rootNode = new Element("main");
	panel.create(rootNode);
	assert.equal(host.document.getElementById("root").parentNode, rootNode);
	panel.show(rootNode);
	const boxes = host.document.querySelectorAll("#slots sp-checkbox");
	assert.equal(boxes.length, 10);
	assert.equal(boxes.filter((box) => box.checked).length, 4);
	assert.equal(host.document.getElementById("doc-line").textContent, "Open a document to export it");
	assert.equal(host.timers.size, 1);
	panel.show(rootNode);
	assert.equal(host.document.getElementById("export").listeners.click.length, 1);
	assert.equal(host.timers.size, 1);
	panel.hide();
	assert.equal(host.timers.size, 0);
	panel.show(rootNode);
	panel.destroy();
	assert.equal(host.timers.size, 0);
});

test("the panel can be shown when the host root is already the plugin root", () => {
	const host = createHarness(ROOT);
	const panel = host.loadHTML().panels.exportKitPanel;
	const root = host.document.getElementById("root");
	assert.doesNotThrow(() => { panel.create(root); panel.show(root); panel.hide(); });
});

test("the menu command initializes its selection before the panel is shown", async () => {
	const host = createHarness(ROOT);
	const command = host.loadHTML().commands.exportKitAll;
	await (typeof command === "function" ? command() : command.run());
	assert.equal(host.document.querySelectorAll("#slots sp-checkbox").length, 10);
	assert.match(host.document.getElementById("status").textContent, /no document/i);
});
