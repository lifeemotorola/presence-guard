/*
Face Retouch for Photoshop
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
with this program; if not, see <https://www.gnu.org/licenses/>.
*/

/*
 * The panel. Thin on purpose: it reads the widgets, asks core.js what that
 * means, hands it to retouch.js, and prints back what Photoshop did. Every
 * decision worth testing is somewhere else.
 */

"use strict";

const { entrypoints } = require("uxp");
const logic = require("./core.js");
const kit = require("./retouch.js");

const STORAGE = {
	preset: "faceretouch.preset",
	strength: "faceretouch.strength",
	texture: "faceretouch.texture",
	toggles: "faceretouch.toggles",
};

const TOGGLES = [
	{ key: "smooth", title: "Even the skin tone", note: "a blurred copy in Colour — pores stay" },
	{ key: "lift", title: "Lift the shadows", note: "under the eyes, beside the nose" },
	{ key: "glow", title: "Glow", note: "soft light on the high points" },
	{ key: "depth", title: "Depth", note: "a little contrast that does not clip" },
	{ key: "warmth", title: "Warmth", note: "takes the grey out of skin" },
	{ key: "detail", title: "Give the texture back", note: "eyelashes, hair, pores" },
];

const state = {
	presetId: logic.DEFAULTS.presetId,
	strength: logic.DEFAULTS.strength,
	texture: logic.DEFAULTS.texture,
	options: Object.assign({}, logic.DEFAULT_OPTIONS),
	busy: false,
	timer: null,
	wired: false,
};

/* ------------------------------------------------------------------------- */
/* small helpers                                                             */
/* ------------------------------------------------------------------------- */

function el(id) {
	return document.getElementById(id);
}

function stored(key, fallback) {
	try {
		const raw = window.localStorage.getItem(key);
		if (raw === null)
			return fallback;

		const value = JSON.parse(raw);
		return value === null || value === undefined ? fallback : value;
	} catch (error) {
		return fallback;
	}
}

function store(key, value) {
	try {
		window.localStorage.setItem(key, JSON.stringify(value));
	} catch (error) {
		/* a panel that cannot remember a slider position still has to work */
	}
}

/* UXP spectrum widgets read `disabled` as an attribute; assigning the property is
 * a no-op in some builds, which is how a panel ends up letting you press a button
 * twice while the first press is still running. */
function enable(node, on) {
	if (!node)
		return;

	if (on)
		node.removeAttribute("disabled");
	else
		node.setAttribute("disabled", "");
}

function setStatus(text, tone) {
	const node = el("status");
	if (!node)
		return;

	node.textContent = text || "";
	node.className = "status" + (tone ? " " + tone : "");
}

/* ------------------------------------------------------------------------- */
/* rendering                                                                 */
/* ------------------------------------------------------------------------- */

function loadSettings() {
	state.presetId = stored(STORAGE.preset, logic.DEFAULTS.presetId);
	state.strength = logic.clamp(Number(stored(STORAGE.strength, logic.DEFAULTS.strength)), 0, 100);
	state.texture = logic.clamp(Number(stored(STORAGE.texture, logic.DEFAULTS.texture)), 0, 100);

	const saved = stored(STORAGE.toggles, null);
	if (saved && typeof saved === "object")
		state.options = Object.assign({}, logic.DEFAULT_OPTIONS, saved);

	if (!logic.getPreset(state.presetId))
		state.presetId = logic.DEFAULTS.presetId;
}

function saveSettings() {
	store(STORAGE.preset, state.presetId);
	store(STORAGE.strength, state.strength);
	store(STORAGE.texture, state.texture);
	store(STORAGE.toggles, state.options);
}

function buildPresets() {
	const holder = el("presets");
	holder.textContent = "";

	for (const preset of logic.PRESETS) {
		const chip = document.createElement("div");
		chip.className = "chip" + (preset.id === state.presetId ? " selected" : "");
		chip.textContent = preset.label;
		chip.setAttribute("data-preset", preset.id);
		chip.addEventListener("click", () => {
			if (state.busy)
				return;

			state.presetId = preset.id;
			saveSettings();
			render();
		});
		holder.appendChild(chip);
	}
}

function buildToggles() {
	const holder = el("toggles");
	holder.textContent = "";

	for (const item of TOGGLES) {
		const on = state.options[item.key] !== false;

		const row = document.createElement("div");
		row.className = "toggle " + (on ? "on" : "off");

		const track = document.createElement("div");
		track.className = "switch";
		const knob = document.createElement("div");
		knob.className = "knob";
		track.appendChild(knob);

		const text = document.createElement("div");
		text.className = "text-block";
		const title = document.createElement("div");
		title.className = "t-title";
		title.textContent = item.title;
		const note = document.createElement("div");
		note.className = "t-note";
		note.textContent = item.note;
		text.appendChild(title);
		text.appendChild(note);

		row.appendChild(track);
		row.appendChild(text);

		row.addEventListener("click", () => {
			if (state.busy)
				return;

			state.options[item.key] = state.options[item.key] === false;
			saveSettings();
			render();
		});

		holder.appendChild(row);
	}
}

function render() {
	const preset = logic.getPreset(state.presetId) || logic.PRESETS[0];

	for (const chip of el("presets").children) {
		const selected = chip.getAttribute("data-preset") === preset.id;
		chip.className = "chip" + (selected ? " selected" : "");
	}

	el("blurb").textContent = preset.blurb;
	el("strength-read").textContent = String(state.strength);
	el("texture-read").textContent = String(state.texture);
	el("strength").value = String(state.strength);
	el("texture").value = String(state.texture);

	const toggleRows = el("toggles").children;
	for (let index = 0; index < toggleRows.length; index++) {
		const key = TOGGLES[index].key;
		const on = state.options[key] !== false;
		toggleRows[index].className = "toggle " + (on ? "on" : "off");
	}

	renderPreview();
}

/** The numbers, live, before anything is created. */
function renderPreview() {
	const info = safeInfo();
	const node = el("preview");

	if (!info) {
		node.textContent = "No document open.";
		return;
	}

	const plan = logic.plan({
		presetId: state.presetId,
		strength: state.strength,
		texture: state.texture,
		width: info.width,
		height: info.height,
		options: state.options,
	});

	if (plan.steps.length === 0) {
		node.textContent = "Every layer is switched off — nothing would be built.";
		return;
	}

	node.textContent = `${plan.steps.length} layers · ${plan.radius}px blur · ${plan.width}×${plan.height} document`;
}

function safeInfo() {
	try {
		return kit.documentInfo();
	} catch (error) {
		return null;
	}
}

function refresh() {
	const info = safeInfo();

	if (!info) {
		el("doc-line").textContent = "Open a photo to start";
		enable(el("apply"), false);
		enable(el("compare"), false);
		enable(el("remove"), false);
		renderPreview();
		return;
	}

	const selection = info.hasSelection ? " · selection active" : "";
	el("doc-line").textContent = `${info.name} · ${info.width}×${info.height}${selection}`;

	enable(el("apply"), !state.busy);
	enable(el("compare"), !state.busy && info.hasRetouch);
	enable(el("remove"), !state.busy && info.hasRetouch);

	renderPreview();
}

function setBusy(busy) {
	state.busy = busy;
	enable(el("apply"), !busy);
	enable(el("compare"), !busy);
	enable(el("remove"), !busy);
	enable(el("strength"), !busy);
	enable(el("texture"), !busy);

	if (!busy)
		refresh();
}

/* ------------------------------------------------------------------------- */
/* the report                                                                */
/* ------------------------------------------------------------------------- */

function renderReport(result) {
	const holder = el("report");
	holder.textContent = "";

	if (!result || !result.steps || result.steps.length === 0)
		return;

	for (const step of result.steps) {
		const row = document.createElement("div");
		row.className = "report-row" + (step.ok ? "" : " failed");

		const dot = document.createElement("div");
		dot.className = "dot";

		const name = document.createElement("div");
		name.className = "report-name";
		name.textContent = step.name.replace(logic.PREFIX, "");

		const note = document.createElement("div");
		note.className = "report-note";
		note.textContent = step.ok ? step.note : "failed";

		row.appendChild(dot);
		row.appendChild(name);
		row.appendChild(note);
		holder.appendChild(row);
	}
}

/* ------------------------------------------------------------------------- */
/* actions                                                                   */
/* ------------------------------------------------------------------------- */

async function applyRetouch() {
	if (state.busy)
		return;

	setBusy(true);
	setStatus("Building the layers…", "");
	el("report").textContent = "";

	try {
		const result = await kit.apply({
			presetId: state.presetId,
			strength: state.strength,
			texture: state.texture,
			options: state.options,
		});

		renderReport(result);

		const built = result.steps.filter((step) => step.ok).length;
		const failed = result.steps.length - built;

		if (built === 0) {
			setStatus("Nothing was built. " + (result.steps[0] ? result.steps[0].message : ""), "bad");
		} else if (failed === 0) {
			setStatus(
				result.grouped
					? `${built} layers in one group. Toggle the group off to compare.`
					: `${built} layers above your photo.`,
				"good",
			);
		} else {
			setStatus(`${built} layers built, ${failed} refused — see the list.`, "bad");
		}
	} catch (error) {
		setStatus(error && error.message ? error.message : String(error), "bad");
	} finally {
		setBusy(false);
	}
}

async function compare() {
	if (state.busy)
		return;

	setBusy(true);

	try {
		const result = await kit.toggleVisibility();

		if (!result.found)
			setStatus("There is no retouch to compare yet.", "");
		else
			setStatus(result.visible ? "Retouch is showing." : "Retouch hidden — press Compare again.", "");
	} catch (error) {
		setStatus(error && error.message ? error.message : String(error), "bad");
	} finally {
		setBusy(false);
	}
}

async function removeRetouch() {
	if (state.busy)
		return;

	setBusy(true);
	el("report").textContent = "";

	try {
		const removed = await kit.removeAll();

		if (removed === 0)
			setStatus("There was nothing of mine to remove.", "");
		else
			setStatus(`Removed ${removed} layer${removed === 1 ? "" : "s"}.`, "good");
	} catch (error) {
		setStatus(error && error.message ? error.message : String(error), "bad");
	} finally {
		setBusy(false);
	}
}

/* ------------------------------------------------------------------------- */
/* wiring                                                                    */
/* ------------------------------------------------------------------------- */

function wireSliders() {
	const strength = el("strength");
	const texture = el("texture");

	strength.value = String(state.strength);
	texture.value = String(state.texture);

	if (state.wired)
		return;

	state.wired = true;

	strength.addEventListener("input", () => {
		state.strength = logic.clamp(Number(strength.value), 0, 100);
		el("strength-read").textContent = String(state.strength);
		saveSettings();
		renderPreview();
	});

	texture.addEventListener("input", () => {
		state.texture = logic.clamp(Number(texture.value), 0, 100);
		el("texture-read").textContent = String(state.texture);
		saveSettings();
		renderPreview();
	});

	el("apply").addEventListener("click", () => applyRetouch());
	el("compare").addEventListener("click", () => compare());
	el("remove").addEventListener("click", () => removeRetouch());
}

function start() {
	loadSettings();
	buildPresets();
	buildToggles();
	wireSliders();
	render();
	refresh();
	setStatus("Ready when you are.", "");

	/* Photoshop has no single event for "a different document became active" that
	 * behaves the same in every 23.x build, so the panel looks every second and a
	 * half. It is three property reads. */
	if (state.timer)
		clearInterval(state.timer);
	state.timer = setInterval(() => {
		if (!state.busy)
			refresh();
	}, 1500);
}

function stop() {
	if (state.timer) {
		clearInterval(state.timer);
		state.timer = null;
	}
}

entrypoints.setup({
	panels: {
		faceRetouchPanel: {
			show() {
				start();
			},
			hide() {
				stop();
			},
		},
	},
	commands: {
		faceRetouchApply: {
			run() {
				return applyRetouch();
			},
		},
	},
});
