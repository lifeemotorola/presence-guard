/*
Export Kit for Photoshop
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
with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/*
 * The panel: ticks, a folder, a button, and an honest account of what happened.
 *
 * Deliberately thin. Anything that decides something lives in core.js, where it
 * is tested; this file only reads the widgets, calls the exporter and prints the
 * result. That keeps the untestable surface — the part that needs Photoshop open
 * in front of you — as small as it can be.
 */

"use strict";

const { entrypoints } = require("uxp");
const logic = require("./core.js");
const kit = require("./photoshop.js");

const KEYS = {
	slots: "exportkit.slots",
	quality: "exportkit.quality",
	sharpen: "exportkit.sharpen",
	smartcrop: "exportkit.smartcrop",
};

const state = {
	folder: null,
	folderRequest: null,
	busy: false,
	lastDoc: "",
	wired: false,
	timer: null,
};

/* ------------------------------------------------------------------------- */
/* small helpers                                                             */
/* ------------------------------------------------------------------------- */

function el(id) {
	return document.getElementById(id);
}

function readStored(key, fallback) {
	try {
		const raw = window.localStorage.getItem(key);
		return raw === null ? fallback : JSON.parse(raw);
	} catch (error) {
		return fallback;
	}
}

function store(key, value) {
	try {
		window.localStorage.setItem(key, JSON.stringify(value));
	} catch (error) {
		/* remembering preferences is a convenience, not a requirement */
	}
}

function setDisabled(button, disabled) {
	if (!button)
		return;

	if (disabled)
		button.setAttribute("disabled", "");
	else
		button.removeAttribute("disabled");
}

function setStatus(text) {
	const node = el("status");
	if (node)
		node.textContent = text || "";
}

function setFolderLine(text) {
	const node = el("folder-line");
	if (node)
		node.textContent = text;
}

/* ------------------------------------------------------------------------- */
/* the slot list                                                             */
/* ------------------------------------------------------------------------- */

function buildSlots() {
	const container = el("slots");
	if (!container)
		return;

	const enabled = readStored(KEYS.slots, logic.defaultSelection());
	const list = Array.isArray(enabled) ? enabled : logic.defaultSelection();

	container.innerHTML = "";

	let currentGroup = null;

	for (const slot of logic.SLOTS) {
		if (slot.group !== currentGroup) {
			currentGroup = slot.group;
			const heading = document.createElement("div");
			heading.className = "group-title";
			heading.textContent = currentGroup;
			container.appendChild(heading);
		}

		const row = document.createElement("div");
		row.className = "slot-row";

		const box = document.createElement("sp-checkbox");
		box.dataset.slotId = slot.id;
		box.textContent = slot.label;
		box.checked = list.indexOf(slot.id) !== -1;
		box.addEventListener("change", () => {
			store(KEYS.slots, readSelection());
			refreshNotes();
		});

		const size = document.createElement("span");
		size.className = "slot-size";
		/* "padded" is the honest word for a fit slot: the whole picture is kept
		 * and the leftover space is filled, it is not cropped to fit. */
		size.textContent = `${slot.width}×${slot.height}${slot.mode === "fit" ? " · padded" : ""}`;

		row.appendChild(box);
		row.appendChild(size);
		container.appendChild(row);
	}
}

function readSelection() {
	const boxes = document.querySelectorAll("#slots sp-checkbox");
	const selection = [];
	for (const box of boxes) {
		if (box.checked && box.dataset.slotId)
			selection.push(box.dataset.slotId);
	}
	return selection;
}

/* ------------------------------------------------------------------------- */
/* options                                                                   */
/* ------------------------------------------------------------------------- */

function readSettings() {
	const quality = Number(el("quality") ? el("quality").value : 10);

	return {
		quality: Number.isFinite(quality) ? logic.clamp(quality, 0, 12) : 10,
		sharpen: Boolean(el("sharpen") && el("sharpen").checked),
		smartCrop: Boolean(el("smartcrop") && el("smartcrop").checked),
	};
}

function wireOptions() {
	const quality = el("quality");
	if (quality) {
		const stored = readStored(KEYS.quality, 10);
		quality.value = String(stored);
		quality.addEventListener("change", () => store(KEYS.quality, readSettings().quality));
	}

	const sharpen = el("sharpen");
	if (sharpen) {
		sharpen.checked = Boolean(readStored(KEYS.sharpen, false));
		sharpen.addEventListener("change", () => store(KEYS.sharpen, Boolean(sharpen.checked)));
	}

	const smartcrop = el("smartcrop");
	if (smartcrop) {
		smartcrop.checked = readStored(KEYS.smartcrop, true) !== false;
		smartcrop.addEventListener("change", () => store(KEYS.smartcrop, Boolean(smartcrop.checked)));
	}
}

/* ------------------------------------------------------------------------- */
/* what the document can and cannot do                                      */
/* ------------------------------------------------------------------------- */

function refreshDocument() {
	try {
		const info = kit.activeDocumentInfo();
		state.lastDoc = info.name;
		el("doc-line").textContent = `${info.name} · ${info.width}×${info.height}`;
		refreshNotes(info);
	} catch (error) {
		el("doc-line").textContent = "Open a document to export it";
		const notes = el("notes");
		if (notes)
			notes.textContent = "";
	}
}

/*
 * Says out loud what the panel is about to do to a small document, before the
 * button is pressed rather than in the exported files.
 */
function refreshNotes(info) {
	const notes = el("notes");
	if (!notes)
		return;

	let document_ = info;
	if (!document_) {
		try {
			document_ = kit.activeDocumentInfo();
		} catch (error) {
			notes.textContent = "";
			return;
		}
	}

	const selection = readSelection();
	if (selection.length === 0) {
		notes.textContent = "No sizes ticked.";
		return;
	}

	const jobs = logic.planExport(document_.width, document_.height, selection, readSettings());
	const worst = jobs.reduce((acc, job) => Math.max(acc, job.upscaleFactor), 0);

	if (worst > 1.5) {
		/* The biggest blow-up is the one worth mentioning: it is the one that
		 * will look soft. */
		const culprits = jobs.filter((job) => job.upscaleFactor === worst).map((job) => job.label);
		notes.textContent = `${culprits.join(", ")} will be enlarged ${worst.toFixed(1)}× from this document.`;
		return;
	}

	notes.textContent = "";
}

/* ------------------------------------------------------------------------- */
/* the export                                                                */
/* ------------------------------------------------------------------------- */

function renderResults(results) {
	const container = el("results");
	if (!container)
		return;

	container.innerHTML = "";

	for (const result of results) {
		const row = document.createElement("div");
		row.className = `result${result.ok ? "" : " failed"}`;

		const name = document.createElement("span");
		name.className = "name";
		name.textContent = result.ok ? result.name : `${result.label}: ${result.error}`;

		const detail = document.createElement("span");
		detail.className = "detail";
		if (result.ok) {
			const details = [];
			const size = logic.formatBytes(result.bytes);
			if (size) details.push(size);
			if (result.sharpened === false) details.push("sharpen skipped");
			if (result.cropDecision) details.push(result.cropDecision);
			detail.textContent = details.join(" · ");
		}

		row.appendChild(name);
		row.appendChild(detail);
		container.appendChild(row);
	}
}

async function ensureFolder() {
	if (state.folder)
		return state.folder;

	const remembered = await kit.rememberedFolder();
	if (remembered && !state.folder) {
		state.folder = remembered;
		setFolderLine(remembered.nativePath || remembered.name || "Folder");
		return state.folder;
	}

	return null;
}

async function pickFolder() {
	if (state.folderRequest)
		return state.folderRequest;
	const request = (async () => {
		try {
			const folder = await kit.chooseFolder();
			if (!folder)
				return;
			state.folder = folder;
			setFolderLine(folder.nativePath || folder.name || "Folder");
		} catch (error) {
			setStatus(String((error && error.message) || error));
		}
	})();
	state.folderRequest = request;
	try { return await request; } finally { state.folderRequest = null; }
}

async function exportEverything() {
	if (state.busy)
		return;

	let info;
	try {
		info = kit.activeDocumentInfo();
	} catch (error) {
		setStatus(String((error && error.message) || error));
		return;
	}

	const selection = readSelection();
	if (selection.length === 0) {
		setStatus("Tick at least one size.");
		return;
	}

	const settings = readSettings();
	const jobs = logic.planExport(info.width, info.height, selection, settings);

	/* Lock before awaiting a folder chooser, not only once export begins. */
	state.busy = true;
	setDisabled(el("export"), true);
	setDisabled(el("choose-folder"), true);
	setStatus("Starting…");

	try {
		if (state.folderRequest)
			await state.folderRequest;
		await ensureFolder();
		if (!state.folder) {
			await pickFolder();
			if (!state.folder) {
				setStatus("Choose where the files should go first.");
				return;
			}
		}
		const outcome = await kit.exportJobs(jobs, {
			folder: state.folder,
			settings,
			baseName: info.name,
			documentId: info.id,
			onProgress: (progress) => {
				setStatus(`Exporting ${progress.index + 1} of ${progress.total} · ${progress.label}`);
			},
		});

		renderResults(outcome.results);
		const summary = logic.summarise(outcome.results).text;
		setStatus(outcome.cancelled ? `Cancelled · ${summary} completed; check the destination for partial files.` : summary);
	} catch (error) {
		setStatus(String((error && error.message) || error));
	} finally {
		state.busy = false;
		setDisabled(el("export"), false);
		setDisabled(el("choose-folder"), false);
	}
}

/* ------------------------------------------------------------------------- */
/* wiring                                                                    */
/* ------------------------------------------------------------------------- */

function wire() {
	if (state.wired)
		return;
	state.wired = true;
	buildSlots();
	wireOptions();

	const choose = el("choose-folder");
	if (choose)
		choose.addEventListener("click", pickFolder);

	const run = el("export");
	if (run)
		run.addEventListener("click", exportEverything);

	refreshDocument();
	ensureFolder().then(() => {
		if (state.folder)
			setFolderLine(state.folder.nativePath || state.folder.name || "Folder");
	});

}

/* Manifest v5 supplies the panel node itself, not { rootNode }. Avoid moving
 * the root into itself if the host has already mounted it. */
function mount(rootNode) {
	const root = el("root");
	if (!root || !rootNode || typeof rootNode.appendChild !== "function")
		return;
	for (let parent = rootNode; parent; parent = parent.parentNode) {
		if (parent === root)
			return;
	}
	if (root.parentNode !== rootNode)
		rootNode.appendChild(root);
}

function start(rootNode) {
	mount(rootNode);
	wire();
	refreshDocument();
	/* Refresh only while visible; repeated show calls must not leak timers. */
	if (!state.timer) {
		state.timer = setInterval(() => {
			if (!state.busy)
				refreshDocument();
		}, 1000);
	}
}

function stop() {
	if (state.timer) {
		clearInterval(state.timer);
		state.timer = null;
	}
}

entrypoints.setup({
	panels: {
		exportKitPanel: {
			create(rootNode) {
				mount(rootNode);
				wire();
			},
			show(rootNode) { start(rootNode); },
			hide() { stop(); },
			destroy() { stop(); },
		},
	},
	commands: {
		exportKitAll() {
			wire();
			return exportEverything();
		},
	},
});
