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
 * Everything that touches Photoshop, and nothing that decides anything.
 *
 * Two rules in here, both learned the hard way from scripts that fail in the
 * middle of somebody's photo:
 *
 *   1. Every step is optional. A filter that refuses on a smart object, a blend
 *      mode an older build spells differently, a group that cannot be made —
 *      each is caught, recorded, and the retouch continues. A half-finished
 *      retouch with a report beats no retouch and a dialog box with a number in
 *      it.
 *   2. Copies are always made from the same original layer and always land
 *      directly above it, which is what fixes the stacking order without ever
 *      moving anything. Grouping happens afterwards, if it happens at all.
 */

"use strict";

const photoshop = require("photoshop");
const { app, core: psCore, action, constants } = photoshop;

const logic = require("./core.js");
const descriptors = require("./descriptors.js");

const BATCH = { synchronousExecution: true };

/*
 * ElementPlacement has been spelled more than one way over the years — the
 * ExtendScript generation called it PLACEINSIDE, the current one INSIDE. Rather
 * than bet the group on the spelling, ask for the one that exists.
 */
function placement(names) {
	const table = constants.ElementPlacement || {};

	for (const name of names) {
		if (table[name])
			return table[name];
	}

	return undefined;
}

const INSIDE = placement(["INSIDE", "PLACEINSIDE"]);
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

/**
 * Builds the layer stack. Must be called inside executeAsModal.
 * Returns a step-by-step report; never leaves a Photoshop error to the caller.
 */
async function build(settings) {
	const doc = activeDocument();
	const width = Math.round(Number(doc.width));
	const height = Math.round(Number(doc.height));

	const plan = logic.plan({
		presetId: settings.presetId,
		strength: settings.strength,
		texture: settings.texture,
		width: width,
		height: height,
		options: settings.options,
	});

	if (plan.steps.length === 0)
		return { plan: plan, steps: [], grouped: false };

	const base = baseLayer(doc);
	const created = [];
	const report = [];

	for (const step of plan.steps) {
		const entry = { id: step.id, name: step.name, note: step.note, ok: false, message: "" };

		const isFill = !!(step.filter && step.filter.type === "fill");

		try {
			/* Always copy from the original, with the original active: the copy
			 * lands directly above it, which is what stacks the layers in plan
			 * order without ever moving one. */
			doc.activeLayers = [base];

			let layer = null;

			if (isFill) {
				/* A solid colour layer is made, not copied; it too lands above the
				 * active layer, so it arrives just under the layers made before it. */
				await action.batchPlay([descriptors.solidColour(step.filter.color, step.name)], BATCH);
				layer = doc.activeLayers[0];
			} else {
				layer = await base.duplicate();
			}

			if (!layer)
				throw new Error("Photoshop did not hand back the new layer.");

			try {
				layer.name = step.name;
			} catch (error) {
				entry.message = `could not rename the layer: ${error.message}`;
			}

			/* The blend mode and the opacity are what make the layer do its job.
			 * A warmth fill left on Normal at 100% would simply paint the photo
			 * orange, so this is set for the fill exactly as for the copies. */
			try {
				layer.blendMode = step.blend;
			} catch (error) {
				entry.message = `blend mode "${step.blend}" not available: ${error.message}`;
			}

			layer.opacity = step.opacity;

			if (!isFill) {
				doc.activeLayers = [layer];

				const descriptor = descriptors.forStep(step);
				if (descriptor)
					await action.batchPlay([descriptor], BATCH);
			}

			created.push(layer);
			entry.ok = true;
		} catch (error) {
			entry.message = error && error.message ? error.message : String(error);
		}

		report.push(entry);
	}

	await order(created);

	const grouped = await groupThem(doc, created);

	return { plan: plan, steps: report, grouped: grouped };
}

/**
 * Puts the new layers in plan order — top layer first — regardless of where
 * Photoshop happened to drop each copy. Moving a layer after its neighbour is the
 * one reordering the DOM does deterministically, and it is a no-op if the copy is
 * already there.
 */
async function order(layers) {
	if (!AFTER || layers.length < 2)
		return;

	for (let index = layers.length - 1; index >= 1; index--) {
		try {
			await layers[index].move(layers[index - 1], AFTER);
		} catch (error) {
			/* a locked or already-correct layer; the next attempt may still work */
		}
	}
}

/**
 * Tries to put the new layers into one group. If Photoshop says no — older
 * builds spell the layer section differently, or the document has a locked
 * arrangement — the layers stay where they are: above the original, correctly
 * ordered, still removable by name.
 */
async function groupThem(doc, created) {
	if (!created.length)
		return false;

	let group = null;

	if (!INSIDE)
		return false;

	try {
		doc.activeLayers = [created[0]];
		await action.batchPlay([descriptors.layerGroup(logic.GROUP_NAME)], BATCH);
		const candidate = doc.activeLayers[0];

		if (candidate && candidate.layers && candidate.layers.length === 0)
			group = candidate;
	} catch (error) {
		return false;
	}

	if (!group)
		return false;

	for (const layer of created) {
		try {
			await layer.move(group, INSIDE);
		} catch (error) {
			/* one layer stays outside; the group is still worth having */
		}
	}

	/* Inside the group the order has to be put back: where a freshly moved layer
	 * lands among its new siblings is not specified anywhere. */
	await order(created);

	try {
		doc.activeLayers = [group];
	} catch (error) {
		/* selection is a nicety */
	}

	return true;
}

/* ------------------------------------------------------------------------- */
/* the three buttons                                                         */
/* ------------------------------------------------------------------------- */

async function apply(settings) {
	return psCore.executeAsModal(async () => build(settings), { commandName: "Face Retouch" });
}

async function removeAll() {
	return psCore.executeAsModal(
		async () => {
			const doc = activeDocument();
			const found = collectRetouchLayers(doc);

			/* Groups first: deleting one takes its members with it, and the
			 * members that are already gone simply refuse. */
			const groups = found.filter((layer) => layer.layers && layer.layers.length);
			const others = found.filter((layer) => !(layer.layers && layer.layers.length));

			let removed = 0;

			for (const layer of groups.concat(others)) {
				try {
					await layer.delete();
					removed++;
				} catch (error) {
					/* already deleted with its parent */
				}
			}

			return removed;
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
