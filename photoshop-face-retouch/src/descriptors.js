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
 * The three things a UXP panel cannot do without batchPlay, written down in one
 * place: a Gaussian blur, a high pass, and a solid colour fill layer.
 *
 * Everything else in this plugin — copying a layer, naming it, choosing a blend
 * mode, setting an opacity, showing or deleting it — is done with the Photoshop
 * DOM, because a DOM call fails loudly and predictably while a wrong descriptor
 * fails with "General Photoshop error occurred" and no line number.
 */

"use strict";

const core = require("./core.js");

const NO_DIALOG = { dialogOptions: "dontDisplay" };

/** Every filter and every "set" in this plugin acts on the active layer. */
function activeLayerTarget() {
	return [{ _ref: "layer", _enum: "ordinal", _value: "targetEnum" }];
}

function pixels(value) {
	return { _unit: "pixelsUnit", _value: Math.round(value * 100) / 100 };
}

function clampRadius(radius) {
	const value = Number(radius);
	if (!Number.isFinite(value))
		return core.RADIUS_MIN;

	return Math.round(core.clamp(value, core.RADIUS_MIN, core.RADIUS_MAX) * 100) / 100;
}

function gaussianBlur(radius) {
	return {
		_obj: "gaussianBlur",
		_target: activeLayerTarget(),
		radius: pixels(clampRadius(radius)),
		_options: NO_DIALOG,
	};
}

function highPass(radius) {
	return {
		_obj: "highPass",
		_target: activeLayerTarget(),
		radius: pixels(clampRadius(radius)),
		_options: NO_DIALOG,
	};
}

function channel(value) {
	const number = Number(value);
	if (!Number.isFinite(number))
		return 0;

	return Math.round(core.clamp(number, 0, 255));
}

/*
 * A solid colour fill layer. "grain" really is the green channel: the descriptor
 * predates anybody caring what the key was called, and it has been that way
 * since Photoshop 6, so it stays.
 */
function solidColour(color, name) {
	const tint = color || {};

	return {
		_obj: "make",
		_target: [{ _ref: "contentLayer" }],
		using: {
			_obj: "contentLayer",
			name: String(name || "Colour Fill"),
			type: {
				_obj: "solidColorLayer",
				color: {
					_obj: "RGBColor",
					red: channel(tint.red),
					grain: channel(tint.green),
					blue: channel(tint.blue),
				},
			},
		},
		_options: NO_DIALOG,
	};
}

/** The empty group the retouch layers are moved into when Photoshop allows it. */
function layerGroup(name) {
	return {
		_obj: "make",
		_target: [{ _ref: "layerSection" }],
		using: {
			_obj: "layerSection",
			name: String(name || core.GROUP_NAME),
		},
		_options: NO_DIALOG,
	};
}

/** From the plan's step to the one descriptor it needs, or null for a plain copy. */
function forStep(step) {
	if (!step || !step.filter)
		return null;

	switch (step.filter.type) {
		case "gaussianBlur":
			return gaussianBlur(step.filter.radius);

		case "highPass":
			return highPass(step.filter.radius);

		case "fill":
			return solidColour(step.filter.color, step.name);

		default:
			return null;
	}
}

module.exports = {
	activeLayerTarget,
	pixels,
	clampRadius,
	gaussianBlur,
	highPass,
	solidColour,
	layerGroup,
	forStep,
};
