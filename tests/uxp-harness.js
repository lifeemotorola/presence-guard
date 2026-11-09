/*
 * A deliberately small UXP simulation for the Photoshop plugin tests.
 *
 * HTML <script> code gets a require rooted at the plugin folder, whereas
 * CommonJS modules loaded by it resolve relative imports from their own file.
 * The DOM and Photoshop objects below are fakes, NOT a Photoshop runtime.
 */
"use strict";

const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

class Element {
	constructor(tag) {
		this.tagName = tag.toLowerCase();
		this.children = [];
		this.parentNode = null;
		this.attributes = {};
		this.dataset = {};
		this.listeners = {};
		this.className = "";
		this.value = "";
		this.checked = false;
		this._text = "";
	}

	set textContent(value) {
		this._text = String(value);
		for (const child of this.children)
			child.parentNode = null;
		this.children = [];
	}
	get textContent() { return this._text; }
	set innerHTML(value) { this.textContent = value; }
	get innerHTML() { return this._text; }
	setAttribute(name, value) { this.attributes[name] = String(value); }
	getAttribute(name) { return Object.hasOwn(this.attributes, name) ? this.attributes[name] : null; }
	removeAttribute(name) { delete this.attributes[name]; }
	addEventListener(name, handler) {
		(this.listeners[name] || (this.listeners[name] = [])).push(handler);
	}
	async dispatch(name) {
		for (const handler of this.listeners[name] || [])
			await handler({ target: this });
	}
	appendChild(child) {
		for (let parent = this; parent; parent = parent.parentNode) {
			if (parent === child)
				throw new Error("Cannot append an ancestor to itself");
		}
		if (child.parentNode) {
			const siblings = child.parentNode.children;
			const index = siblings.indexOf(child);
			if (index >= 0)
				siblings.splice(index, 1);
		}
		child.parentNode = this;
		this.children.push(child);
		return child;
	}
}

function fakeDocument(html) {
	const elements = new Map();
	for (const match of html.matchAll(/<([\w-]+)\b([^>]*\bid="([^"]+)"[^>]*)>/g)) {
		const node = new Element(match[1]);
		const value = /\bvalue="([^"]*)"/.exec(match[2]);
		if (value)
			node.value = value[1];
		node.checked = /\schecked(?:\s|>|$)/.test(match[2]);
		elements.set(match[3], node);
	}
	const body = new Element("body");
	const root = elements.get("root");
	if (root) {
		body.appendChild(root);
		for (const [id, node] of elements) {
			if (id !== "root")
				root.appendChild(node);
		}
	}
	return {
		body,
		createElement: (tag) => new Element(tag),
		getElementById: (id) => elements.get(id) || null,
		querySelectorAll(selector) {
			const match = /^#([\w-]+)\s+([\w-]+)$/.exec(selector);
			if (!match)
				throw new Error(`The fake DOM does not support selector ${selector}`);
			const result = [];
			const walk = (node) => {
				if (!node) return;
				for (const child of node.children) {
					if (child.tagName === match[2].toLowerCase()) result.push(child);
					walk(child);
				}
			};
			walk(elements.get(match[1]));
			return result;
		},
	};
}

function createHarness(pluginRoot, options = {}) {
	const root = path.resolve(pluginRoot);
	const manifest = JSON.parse(fs.readFileSync(path.join(root, "manifest.json"), "utf8"));
	const html = fs.readFileSync(path.join(root, manifest.main), "utf8");
	const document = fakeDocument(html);
	const timers = new Map();
	const storage = new Map();
	const setups = [];
	let nextTimer = 1;
	const photoshop = options.photoshop || {
		app: { activeDocument: null },
		constants: { ElementPlacement: { PLACEINSIDE: "placeInside", PLACEAFTER: "placeAfter" } },
		core: { executeAsModal: async (fn) => fn({ isCancelled: false }) },
		action: { batchPlay: async () => [] },
	};
	const uxp = {
		entrypoints: { setup: (config) => setups.push(config) },
		storage: { localFileSystem: Object.assign({
			getFolder: async () => null,
			createPersistentToken: async () => "token",
			getEntryForPersistentToken: async () => null,
		}, options.localFileSystem || {}) },
	};
	const context = vm.createContext({
		console,
		document,
		window: { localStorage: {
			getItem: (key) => storage.has(key) ? storage.get(key) : null,
			setItem: (key, value) => storage.set(key, String(value)),
		} },
		setInterval: (fn, ms) => { const id = nextTimer++; timers.set(id, { fn, ms }); return id; },
		clearInterval: (id) => timers.delete(id),
	});
	const cache = new Map();

	function makeRequire(parentDir) {
		return function requireUXP(name) {
			if (name === "uxp") return uxp;
			if (name === "photoshop") return photoshop;
			if (!name.startsWith(".")) throw new Error(`Unknown mocked UXP module: ${name}`);
			let file = path.resolve(parentDir, name);
			if (!path.extname(file)) file += ".js";
			if (!file.startsWith(root + path.sep)) throw new Error(`Import escapes plugin folder: ${name}`);
			if (!fs.existsSync(file)) throw new Error(`Module not found: ${name} from ${path.relative(root, parentDir) || "."}`);
			if (cache.has(file)) return cache.get(file).exports;
			const module = { exports: {} };
			cache.set(file, module);
			const source = fs.readFileSync(file, "utf8");
			const fn = vm.runInContext(`(function(require, module, exports) {\n${source}\n})`, context, { filename: file });
			fn(makeRequire(path.dirname(file)), module, module.exports);
			return module.exports;
		};
	}

	return {
		manifest, html, document, timers, photoshop, setups,
		loadModule: (name) => makeRequire(root)(name),
		loadHTML() {
			const scripts = [...html.matchAll(/<script\b[^>]*\bsrc="([^"]+)"[^>]*><\/script>/g)];
			if (!scripts.length) throw new Error("No HTML entry script found");
			context.require = makeRequire(root);
			for (const match of scripts) {
				const file = path.resolve(root, match[1]);
				if (!file.startsWith(root + path.sep)) throw new Error("HTML script escapes plugin folder");
				vm.runInContext(fs.readFileSync(file, "utf8"), context, { filename: file });
			}
			return setups[0];
		},
	};
}

module.exports = { createHarness, Element };
