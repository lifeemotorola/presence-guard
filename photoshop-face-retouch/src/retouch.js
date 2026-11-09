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

/* Photoshop-facing adapter. Its modal operations use batchPlay for layer
 * selection because Document.activeLayers is read-only before Photoshop 26.9.
 * New layers are hidden or at 0% opacity while configured. Photoshop requires
 * filter targets to be visible, so filter copies are shown at 0% first. Failed
 * steps are removed or left inert with a warning, never applied to the original. */

"use strict";

const photoshop = require("photoshop");
const { app, core: psCore, action, constants } = photoshop;

const logic = require("./core.js");
const descriptors = require("./descriptors.js");

const BATCH = { synchronousExecution: true };

/*
 * Prefer UXP's documented PLACEINSIDE / PLACEAFTER, with legacy aliases as
 * fallbacks. Grouping is optional if the host cannot move the new layers.
 */
function placement(names) {
	const table = constants.ElementPlacement || {};

	for (const name of names) {
		if (table[name])
			return table[name];
	}

	return undefined;
}

const INSIDE = placement(["PLACEINSIDE", "INSIDE"]);
const AFTER = placement(["PLACEAFTER", "PLACE_AFTER", "AFTER"]);

/* ------------------------------------------------------------------------- */
/* reading the document                                                      */
/* ------------------------------------------------------------------------- */

function activeDocument() {
	const doc = app.activeDocument;
	if (!doc)
		throw new Error("Open a photo first — there is no document.");
	return doc;
}

function documentInfo() {
	const doc = app.activeDocument;
	if (!doc)
		return null;

	return {
		id: doc.id,
		name: doc.name || "Untitled",
		width: Math.round(Number(doc.width)),
		height: Math.round(Number(doc.height)),
		hasRetouch: collectRetouchLayers(doc).length > 0,
		hasSelection: selectionExists(doc),
	};
}

function selectionExists(doc) {
	try {
		const bounds = doc.selection.bounds;
		if (!bounds)
			return false;

		return Number(bounds.right) - Number(bounds.left) > 0 && Number(bounds.bottom) - Number(bounds.top) > 0;
	} catch (error) {
		/* The DOM throws when nothing is selected, which is the answer. */
		return false;
	}
}

function collectRetouchLayers(doc) {
	const found = [];

	const walk = (container) => {
		const layers = container && container.layers ? container.layers : [];
		for (const layer of layers) {
			if (logic.isRetouchLayer(layer.name))
				found.push(layer);

			if (layer.layers && layer.layers.length)
				walk(layer);
		}
	};

	walk(doc);
	return found;
}

/** The layer a retouch is built from: what is selected, unless that is a retouch. */
function baseLayer(doc) {
	const active = doc.activeLayers && doc.activeLayers.length ? doc.activeLayers[0] : null;
	if (active && !logic.isRetouchLayer(active.name))
		return active;

	for (const layer of doc.layers) {
		if (!logic.isRetouchLayer(layer.name))
			return layer;
	}

	throw new Error("There is no layer to retouch — the document only holds a previous retouch.");
}

/* ------------------------------------------------------------------------- */
/* the retouch                                                               */
/* ------------------------------------------------------------------------- */

/* batchPlay can resolve successfully with an error descriptor; treating that
 * as success could apply a filter to the wrong layer or leave a solid tint. */
async function playChecked(descriptor) {
	const result = await action.batchPlay([descriptor], BATCH);
	for (const item of result || []) {
		if (item && String(item._obj || "").toLowerCase() === "error") {
			const error = new Error(item.message || `Photoshop error ${item.result}`);
			error.code = item.result;
			throw error;
		}
	}
}

async function selectLayer(doc, layer, context) {
	if (!layer || layer.id === undefined || doc.id === undefined)
		throw new Error("Photoshop did not provide a layer or document ID.");
	if (!app.activeDocument || app.activeDocument.id !== doc.id)
		throw new Error("The active document changed during the retouch.");
	const descriptor = {
		_obj: "select",
		_target: [{ _ref: "layer", _id: layer.id }, { _ref: "document", _id: doc.id }],
		makeVisible: false,
		_options: { dialogOptions: "dontDisplay" },
	};
	try {
		await playChecked(descriptor);
	} catch (error) {
		if (cancelled(context, error) || !app.activeDocument || app.activeDocument.id !== doc.id)
			throw error;
		/* Some Photoshop actions reject the optional document reference. The
		 * document is already active and modal; retry the ID-only form. */
		descriptor._target = [{ _ref: "layer", _id: layer.id }];
		await playChecked(descriptor);
	}
	const selected = doc.activeLayers || [];
	if (!app.activeDocument || app.activeDocument.id !== doc.id ||
		!Array.from(selected).some((item) => item.id === layer.id))
		throw new Error("Photoshop did not select the intended layer.");
}

function layerIds(container, ids) {
	const found = ids || new Set();
	for (const layer of (container && container.layers) || []) {
		found.add(layer.id);
		if (layer.layers && layer.layers.length)
			layerIds(layer, found);
	}
	return found;
}

function cancelled(context, error) {
	return Boolean((context && context.isCancelled) || (error && error.code === -128));
}

/* Cleanup can itself fail, especially after Escape. Preserve an already-hidden
 * incomplete layer when possible and say explicitly what the user should check. */
async function discardFailedLayer(layer, safelyInvisible) {
	if (!layer)
		return "";
	try {
		await layer.delete();
		return "";
	} catch (error) {
		if (safelyInvisible) {
			try { layer.visible = false; } catch (ignored) { /* opacity was already zero */ }
			return "; incomplete layer was left hidden or at 0% opacity — remove it manually";
		}
		try {
			layer.visible = false;
			return "; incomplete layer was left hidden — remove it manually";
		} catch (ignored) {
			return "; could not remove or hide the incomplete layer — undo or inspect it manually";
		}
	}
}

/** Build the stack inside executeAsModal and report every attempted step. */
async function build(settings, context) {
	const doc = activeDocument();
	const plan = logic.plan({
		presetId: settings.presetId,
		strength: settings.strength,
		texture: settings.texture,
		width: Math.round(Number(doc.width)),
		height: Math.round(Number(doc.height)),
		options: settings.options,
	});

	if (plan.steps.length === 0)
		return { plan: plan, steps: [], grouped: false, cancelled: false };

	const base = baseLayer(doc);
	const created = [];
	const report = [];
	let wasCancelled = false;

	for (const step of plan.steps) {
		if (cancelled(context)) {
			wasCancelled = true;
			break;
		}
		const entry = { id: step.id, name: step.name, note: step.note, ok: false, message: "" };
		let layer = null;
		let safelyInvisible = false;
		try {
			/* Select by IDs, not the setter introduced in Photoshop 26.9. */
			await selectLayer(doc, base, context);
			const before = layerIds(doc);
			let candidate;
			if (step.filter && step.filter.type === "fill") {
				await playChecked(descriptors.solidColour(step.filter.color, step.name));
				candidate = doc.activeLayers && doc.activeLayers[0];
			} else {
				candidate = await base.duplicate();
			}
			/* Never alter or delete a pre-existing layer if the host returns the
			 * wrong active object after a failed create. */
			if (!candidate || candidate.id === undefined || before.has(candidate.id))
				throw new Error("Photoshop did not return a new layer.");
			layer = candidate;
			layer.visible = false;
			safelyInvisible = true;
			layer.opacity = 0;
			layer.name = step.name;
			/* Prefer the host's enum values; the core plan is host-independent. */
			const modes = constants && constants.BlendMode;
			layer.blendMode = (modes && modes[step.blend.toUpperCase()]) || step.blend;

			/* A hidden layer cannot receive filters in Photoshop. Visible at 0%
			 * remains inert even if cancellation prevents later cleanup. */
			layer.visible = true;
			if (!(step.filter && step.filter.type === "fill")) {
				await selectLayer(doc, layer, context);
				const descriptor = descriptors.forStep(step);
				if (descriptor)
					await playChecked(descriptor);
			}

			layer.opacity = step.opacity;
			safelyInvisible = false;
			created.push(layer);
			entry.ok = true;
		} catch (error) {
			entry.message = (error && error.message ? error.message : String(error)) +
				await discardFailedLayer(layer, safelyInvisible);
			wasCancelled = cancelled(context, error);
		}
		report.push(entry);
		if (wasCancelled)
			break;
	}

	let grouped = false;
	let warning = "";
	if (!wasCancelled && !cancelled(context)) {
		try {
			const ordered = await order(created, context);
			const grouping = await groupThem(doc, created, context);
			grouped = grouping.grouped;
			if (created.length && !grouped)
				warning = "Some layers could not be grouped; check their order in the Layers panel.";
			else if (!(grouped ? grouping.ordered : ordered))
				warning = "Photoshop could not arrange every layer; check their order in the Layers panel.";
		} catch (error) {
			if (cancelled(context, error))
				wasCancelled = true;
			else
				warning = "Could not finish arranging the new layers; check the Layers panel.";
		}
	} else {
		wasCancelled = true;
	}
	if (cancelled(context))
		wasCancelled = true;
	return { plan: plan, steps: report, grouped: grouped, cancelled: wasCancelled, warning: warning };
}

function checkCancellation(context) {
	if (cancelled(context)) {
		const error = new Error("Retouch cancelled.");
		error.code = -128;
		throw error;
	}
}

/** Put new layers in plan order, top first. Report a refused move. */
async function order(layers, context) {
	if (layers.length < 2)
		return true;
	if (!AFTER)
		return false;
	let ordered = true;
	for (let index = 1; index < layers.length; index++) {
		checkCancellation(context);
		try {
			await layers[index].move(layers[index - 1], AFTER);
		} catch (error) {
			if (cancelled(context, error)) throw error;
			ordered = false;
		}
	}
	return ordered;
}

/** Try grouping; do not claim success if any new layer remains outside. */
async function groupThem(doc, created, context) {
	if (!created.length || !INSIDE)
		return { grouped: false, ordered: false };
	checkCancellation(context);
	let group = null;
	try {
		await selectLayer(doc, created[0], context);
		const before = layerIds(doc);
		await playChecked(descriptors.layerGroup(logic.GROUP_NAME));
		const candidate = doc.activeLayers && doc.activeLayers[0];
		if (candidate && !before.has(candidate.id) && candidate.layers && candidate.layers.length === 0)
			group = candidate;
	} catch (error) {
		if (cancelled(context, error)) throw error;
		return { grouped: false, ordered: false };
	}
	if (!group)
		return { grouped: false, ordered: false };

	let allGrouped = true;
	for (const layer of created) {
		checkCancellation(context);
		try {
			await layer.move(group, INSIDE);
			if (!Array.from(group.layers).some((item) => item.id === layer.id))
				allGrouped = false;
		} catch (error) {
			if (cancelled(context, error)) throw error;
			allGrouped = false;
		}
	}
	if (!allGrouped) {
		if (group.layers.length === 0) {
			try { await group.delete(); } catch (error) {
				if (cancelled(context, error)) throw error;
			}
		}
		return { grouped: false, ordered: false };
	}
	const ordered = await order(created, context);
	try { await selectLayer(doc, group, context); } catch (error) {
		if (cancelled(context, error)) throw error;
		/* Final selection is optional. */
	}
	return { grouped: true, ordered: ordered };
}

/* ------------------------------------------------------------------------- */
/* the three buttons                                                         */
/* ------------------------------------------------------------------------- */

async function apply(settings) {
	return psCore.executeAsModal(async (context) => build(settings, context), { commandName: "Face Retouch" });
}

async function removeAll(expectedDocumentId) {
	return psCore.executeAsModal(
		async (context) => {
			const doc = activeDocument();
			if (expectedDocumentId === undefined || doc.id !== expectedDocumentId)
				throw new Error("The active document changed. Review the layers and confirm Remove again.");

			/* If a matching parent is removed, do not attempt to delete its
			 * already-removed children separately. A nonmatching parent is traversed. */
			const targets = [];
			const walk = (container) => {
				for (const layer of (container && container.layers) || []) {
					if (logic.isRetouchLayer(layer.name))
						targets.push(layer);
					else if (layer.layers && layer.layers.length)
						walk(layer);
				}
			};
			walk(doc);
			let removed = 0;
			let failed = 0;
			let wasCancelled = false;
			for (const layer of targets) {
				if (cancelled(context)) { wasCancelled = true; break; }
				try {
					await layer.delete();
					removed++;
				} catch (error) {
					if (cancelled(context, error)) { wasCancelled = true; break; }
					failed++;
				}
			}
			return { removed: removed, failed: failed, cancelled: wasCancelled };
		},
		{ commandName: "Remove Face Retouch" },
	);
}

/** Returns the visibility the retouch was set to, so the button can say so. */
async function toggleVisibility() {
	return psCore.executeAsModal(
		async () => {
			const doc = activeDocument();
			const found = collectRetouchLayers(doc);

			if (!found.length)
				return { found: 0, visible: true };

			const visible = !found[0].visible;

			for (const layer of found) {
				try {
					layer.visible = visible;
				} catch (error) {
					/* a locked layer keeps its own mind */
				}
			}

			return { found: found.length, visible: visible };
		},
		{ commandName: "Compare Retouch" },
	);
}

module.exports = {
	documentInfo,
	apply,
	removeAll,
	toggleVisibility,
	/* exported for the panel's "can I?" checks */
	collectRetouchLayers,
};
