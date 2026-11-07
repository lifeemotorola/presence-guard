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
 * What the retouch actually is, as numbers.
 *
 * No Photoshop in here: this file turns "Glamour, strength 70, on a 24 megapixel
 * photo" into a list of layers with radii, blend modes and opacities. That makes
 * the one part of a retoucher that is easy to get subtly wrong — the arithmetic
 * that decides how much smoothing a 600px crop and a 6000px file each need —
 * something a test can check on a laptop with no Photoshop installed.
 *
 * The whole retouch is layer stack, never a flattened result:
 *
 *   FR · Detail    high pass, overlay      — puts eyelashes, hair and pores back
 *   FR · Warmth    soft light colour       — takes the grey out of skin
 *   FR · Depth     soft light self-blend   — a little contrast, no clipping
 *   FR · Glow      screen, heavy blur      — the soft "lit from inside" look
 *   FR · Lift      lighten, big blur       — opens shadows under the eyes
 *   FR · Smooth    colour, medium blur     — evens blotchy tone, keeps texture
 *   (your original layer)
 *
 * Blurring a copy in *Colour* is the trick that makes this not look plastic:
 * luminosity — pores, stubble, freckles, the actual texture — is left alone, and
 * only the colour blotches get averaged. The Detail layer then restores the
 * high-frequency structure that the eye reads as "real skin".
 */

"use strict";

const PREFIX = "FR · ";
const GROUP_NAME = "Face Retouch";

/* The reference size the radii below are quoted at. Everything is scaled from
 * this, because 2.5px of blur is a lot on a 600px web crop and invisible on a
 * 45 megapixel raw. */
const REFERENCE_SIDE = 1800;

/* Sensible-looking limits, in pixels, after scaling. */
const RADIUS_MIN = 0.3;
const RADIUS_MAX = 60;

const PRESETS = [
	{
		id: "natural",
		label: "Natural",
		blurb: "Keeps pores and freckles. The one to use when nobody should notice.",
		smoothRadius: 5.5,
		colourOpacity: 55,
		liftOpacity: 14,
		glowOpacity: 8,
		depthOpacity: 16,
		warmthOpacity: 8,
		detailOpacity: 45,
		tint: { red: 255, green: 216, blue: 186 },
	},
	{
		id: "soft",
		label: "Soft",
		blurb: "A magazine kind of smooth, still with texture in the shadows.",
		smoothRadius: 8,
		colourOpacity: 65,
		liftOpacity: 20,
		glowOpacity: 12,
		depthOpacity: 18,
		warmthOpacity: 10,
		detailOpacity: 38,
		tint: { red: 255, green: 210, blue: 178 },
	},
	{
		id: "glamour",
		label: "Glamour",
		blurb: "Event photos and head shots: maximum smoothing, warm, glowing.",
		smoothRadius: 12,
		colourOpacity: 74,
		liftOpacity: 26,
		glowOpacity: 18,
		depthOpacity: 22,
		warmthOpacity: 14,
		detailOpacity: 30,
		tint: { red: 255, green: 205, blue: 168 },
	},
	{
		id: "editorial",
		label: "Editorial",
		blurb: "Fashion and documentary: barely smooths, keeps the character.",
		smoothRadius: 3.5,
		colourOpacity: 40,
		liftOpacity: 10,
		glowOpacity: 6,
		depthOpacity: 14,
		warmthOpacity: 6,
		detailOpacity: 60,
		tint: { red: 255, green: 222, blue: 198 },
	},
	{
		id: "studio",
		label: "Studio",
		blurb: "Clean commercial skin: even tone, flat light, no glow.",
		smoothRadius: 7,
		colourOpacity: 62,
		liftOpacity: 22,
		glowOpacity: 4,
		depthOpacity: 20,
		warmthOpacity: 9,
		detailOpacity: 50,
		tint: { red: 255, green: 214, blue: 184 },
	},
];

const DEFAULT_OPTIONS = {
	smooth: true,
	lift: true,
	glow: true,
	depth: true,
	warmth: true,
	detail: true,
};

const DEFAULTS = {
	presetId: "natural",
	strength: 60,
	texture: 50,
};

function clamp(value, low, high) {
	if (!Number.isFinite(value))
		return low;
	return Math.min(high, Math.max(low, value));
}

/** One decimal is plenty for a blur radius, and it reads better in the log. */
function round1(value) {
	return Math.round(value * 10) / 10;
}

function getPreset(id) {
	for (const preset of PRESETS) {
		if (preset.id === id)
			return preset;
	}
	return null;
}

/**
 * How much the radii grow for a document that is not 1800px on its short side.
 * A phone crop gets less blur or it turns to soup; a 45MP file gets more, or the
 * retouch does nothing at 100%.
 */
function scaleFor(width, height) {
	const side = Math.min(Math.abs(Number(width) || 0), Math.abs(Number(height) || 0));
	if (!(side > 0))
		return 1;

	return clamp(side / REFERENCE_SIDE, 0.45, 2.6);
}

function strengthFactor(strength) {
	/* Even at strength 0 a retouch is allowed a whisper of work: the sliders are
	 * a multiplier, not an off switch. The toggles are the off switches. */
	return 0.3 + 0.7 * (clamp(Number(strength), 0, 100) / 100);
}

function textureFactor(texture) {
	return clamp(Number(texture), 0, 100) / 100;
}

function smoothingRadius(preset, strength, width, height) {
	const radius = preset.smoothRadius * scaleFor(width, height) * strengthFactor(strength);
	return round1(clamp(radius, RADIUS_MIN, RADIUS_MAX));
}

function opacity(base, strength) {
	return Math.round(clamp(base * strengthFactor(strength), 1, 100));
}

/** The high pass radius is about "how big is a pore", so it barely moves. */
function detailRadius(preset, width, height) {
	const radius = 1.2 + preset.smoothRadius * 0.12;
	return round1(clamp(radius * Math.sqrt(scaleFor(width, height)), 0.5, 8));
}

/**
 * The plan. Ordered top layer first, because every copy is inserted directly
 * above your original — creating them top-down leaves the stack the right way
 * up whether or not the group below is ever made.
 */
function plan(options) {
	const settings = options || {};
	const preset = getPreset(settings.presetId) || getPreset(DEFAULTS.presetId);
	const enabled = Object.assign({}, DEFAULT_OPTIONS, settings.options || {});
	const width = settings.width;
	const height = settings.height;
	const strength = clamp(Number.isFinite(settings.strength) ? settings.strength : DEFAULTS.strength, 0, 100);
	const texture = clamp(Number.isFinite(settings.texture) ? settings.texture : DEFAULTS.texture, 0, 100);

	const smooth = smoothingRadius(preset, strength, width, height);
	const steps = [];

	if (enabled.detail && texture > 0) {
		steps.push({
			id: "detail",
			name: PREFIX + "Detail",
			blend: "overlay",
			opacity: Math.round(clamp((preset.detailOpacity * texture) / 100, 1, 100)),
			filter: { type: "highPass", radius: detailRadius(preset, width, height) },
			note: "texture and eyelashes back",
		});
	}

	if (enabled.warmth) {
		steps.push({
			id: "warmth",
			name: PREFIX + "Warmth",
			blend: "softLight",
			opacity: opacity(preset.warmthOpacity, strength),
			filter: { type: "fill", color: preset.tint },
			note: "warm tint on the skin",
		});
	}

	if (enabled.depth) {
		steps.push({
			id: "depth",
			name: PREFIX + "Depth",
			blend: "softLight",
			opacity: opacity(preset.depthOpacity, strength),
			filter: { type: "none" },
			note: "contrast without clipping",
		});
	}

	if (enabled.glow) {
		steps.push({
			id: "glow",
			name: PREFIX + "Glow",
			blend: "screen",
			opacity: opacity(preset.glowOpacity, strength),
			filter: { type: "gaussianBlur", radius: round1(clamp(smooth * 3.4, RADIUS_MIN, RADIUS_MAX)) },
			note: "soft light on the high points",
		});
	}

	if (enabled.lift) {
		steps.push({
			id: "lift",
			name: PREFIX + "Lift",
			blend: "lighten",
			opacity: opacity(preset.liftOpacity, strength),
			filter: { type: "gaussianBlur", radius: round1(clamp(smooth * 2.2, RADIUS_MIN, RADIUS_MAX)) },
			note: "shadow lift under the eyes",
		});
	}

	if (enabled.smooth) {
		steps.push({
			id: "smooth",
			name: PREFIX + "Smooth",
			blend: "color",
			opacity: opacity(preset.colourOpacity, strength),
			filter: { type: "gaussianBlur", radius: smooth },
			note: "evens the tone, keeps the texture",
		});
	}

	return {
		preset: preset,
		width: Number(width) || 0,
		height: Number(height) || 0,
		strength: strength,
		texture: texture,
		scale: scaleFor(width, height),
		radius: smooth,
		steps: steps,
		summary: summarise(preset, steps, smooth),
	};
}

function summarise(preset, steps, radius) {
	if (steps.length === 0)
		return "Every layer type is switched off — nothing to do.";

	const names = steps.map((step) => step.name).join(", ");
	return `${preset.label}: ${steps.length} layers (${names}) at ${radius}px.`;
}

function isRetouchLayer(name) {
	if (typeof name !== "string")
		return false;

	return name === GROUP_NAME || (name.length > PREFIX.length && name.indexOf(PREFIX) === 0);
}

/** True when a document already carries a retouch, so the panel can offer Remove. */
function hasRetouch(names) {
	if (!Array.isArray(names))
		return false;

	return names.some((name) => isRetouchLayer(name));
}

module.exports = {
	PREFIX,
	GROUP_NAME,
	REFERENCE_SIDE,
	RADIUS_MIN,
	RADIUS_MAX,
	PRESETS,
	DEFAULT_OPTIONS,
	DEFAULTS,
	clamp,
	round1,
	getPreset,
	scaleFor,
	smoothingRadius,
	detailRadius,
	plan,
	summarise,
	isRetouchLayer,
	hasRetouch,
};
