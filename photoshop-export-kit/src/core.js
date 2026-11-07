/*
Export Kit for Photoshop
Copyright (C) 2026 Presence Guard contributors

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
 * The arithmetic behind Export Kit.
 *
 * Nothing here touches Photoshop: it takes a document size, a slot and a few
 * options and answers "what exactly do I resize to, what do I crop, and what do
 * I call the file". That is the part that can be tested on any machine, and the
 * part that must not be wrong — a wrong crop is a cut-off face on somebody's
 * thumbnail.
 */

"use strict";

/* The sizes that actually matter, in the order the panel shows them. */
const SLOTS = [
	{
		id: "yt-thumb",
		group: "Video",
		label: "YouTube thumbnail",
		width: 1280,
		height: 720,
		mode: "fill",
		format: "jpeg",
	},
	{
		id: "yt-endcard",
		group: "Video",
		label: "YouTube end card",
		width: 1920,
		height: 1080,
		mode: "fit",
		format: "jpeg",
	},
	{
		id: "ig-post",
		group: "Social",
		label: "Instagram post",
		width: 1080,
		height: 1080,
		mode: "fill",
		format: "jpeg",
	},
	{
		id: "ig-story",
		group: "Social",
		label: "Story, Shorts, TikTok",
		width: 1080,
		height: 1920,
		mode: "fill",
		format: "jpeg",
	},
	{
		id: "x-post",
		group: "Social",
		label: "X post",
		width: 1600,
		height: 900,
		mode: "fill",
		format: "jpeg",
	},
	{
		id: "yt-banner",
		group: "Banners",
		label: "YouTube banner",
		width: 2560,
		height: 1440,
		mode: "fill",
		format: "jpeg",
	},
	{
		id: "twitch-banner",
		group: "Banners",
		label: "Twitch banner",
		width: 1200,
		height: 480,
		mode: "fill",
		format: "jpeg",
	},
	{
		id: "x-header",
		group: "Banners",
		label: "X header",
		width: 1500,
		height: 500,
		mode: "fill",
		format: "jpeg",
	},
	{
		id: "twitch-panel",
		group: "Extras",
		label: "Twitch panel",
		width: 320,
		height: 100,
		mode: "fit",
		format: "png",
	},
	{
		id: "discord-emoji",
		group: "Extras",
		label: "Discord emoji",
		width: 128,
		height: 128,
		mode: "fit",
		format: "png",
	},
];

/* What a fresh install ticks: the four sizes a streamer needs every day. */
const DEFAULT_SELECTION = ["yt-thumb", "ig-post", "ig-story", "x-post"];

const DEFAULT_OPTIONS = {
	quality: 10, // JPEG 0..12, Photoshop's own scale
	sharpen: false,
	smartCrop: true,
};

function getSlot(id) {
	return SLOTS.find((slot) => slot.id === id) || null;
}

function defaultSelection() {
	return DEFAULT_SELECTION.slice();
}

function clamp(value, low, high) {
	return Math.min(high, Math.max(low, value));
}

/*
 * Turns a document size into the exact instructions for one slot.
 *
 * fill  — cover the slot, then crop the overflow (nothing is letterboxed)
 * fit   — the whole image inside the slot, then pad (nothing is lost)
 */
function planJob(slot, docWidth, docHeight, options) {
	if (!slot)
		throw new Error("unknown slot");

	if (!Number.isFinite(docWidth) || !Number.isFinite(docHeight) || docWidth <= 0 || docHeight <= 0)
		throw new Error("the document has no size");

	const opts = Object.assign({}, DEFAULT_OPTIONS, options || {});
	const target = { width: slot.width, height: slot.height };
	const mode = slot.mode === "fit" ? "fit" : "fill";

	const scale =
		mode === "fill"
			? Math.max(target.width / docWidth, target.height / docHeight)
			: Math.min(target.width / docWidth, target.height / docHeight);

	let resizeWidth = Math.round(docWidth * scale);
	let resizeHeight = Math.round(docHeight * scale);

	/* Rounding must never leave the image a pixel short of the slot: that is
	 * a one pixel transparent seam in a JPEG, and nobody can explain those. */
	if (mode === "fill") {
		resizeWidth = Math.max(target.width, resizeWidth);
		resizeHeight = Math.max(target.height, resizeHeight);
	}

	const job = {
		slotId: slot.id,
		label: slot.label,
		ext: slot.format === "png" ? "png" : "jpg",
		mode,
		source: { width: docWidth, height: docHeight },
		target,
		resize: { width: resizeWidth, height: resizeHeight },
		scale,
		upscale: scale > 1.0001,
		upscaleFactor: scale,
		slackX: 0,
		slackY: 0,
		crop: null,
		anchor: "MIDDLECENTER",
		warnings: [],
	};

	if (mode === "fill") {
		job.slackX = resizeWidth - target.width;
		job.slackY = resizeHeight - target.height;
		job.crop = {
			left: Math.floor(job.slackX / 2),
			top: Math.floor(job.slackY / 2),
			right: Math.floor(job.slackX / 2) + target.width,
			bottom: Math.floor(job.slackY / 2) + target.height,
		};
	}

	if (job.upscale && job.upscaleFactor > 1.5) {
		job.warnings.push(
			`this is a ${job.upscaleFactor.toFixed(1)}× blow-up from ${docWidth}×${docHeight}: it will look soft`,
		);
	}

	if (opts.sharpen && job.upscale) {
		job.warnings.push("sharpening an enlarged image will exaggerate the softness");
	}

	return job;
}

/*
 * Moves the fill crop inside the slack the resize left over. Used by the smart
 * crop: `offsetX`/`offsetY` are in pixels of the resized document.
 */
function withCropOffset(job, offsetX, offsetY) {
	if (!job || job.mode !== "fill" || !job.crop)
		return job;

	const x = Math.round(clamp(offsetX, 0, job.slackX));
	const y = Math.round(clamp(offsetY, 0, job.slackY));

	return Object.assign({}, job, {
		crop: {
			left: x,
			top: y,
			right: x + job.target.width,
			bottom: y + job.target.height,
		},
	});
}

/* `recording.psd` / `My Stream - Final Final` -> something safe on disk. */
function sanitiseBaseName(name, limit) {
	const max = limit || 60;
	let base = String(name == null ? "" : name)
		.replace(/\.[^.]+$/, "")
		.toLowerCase()
		.normalize("NFKD")
		.replace(/[\u0300-\u036f]/g, "")
		.replace(/[^a-z0-9_-]+/g, "-")
		.replace(/-{2,}/g, "-")
		.replace(/^[-_]+|[-_]+$/g, "");

	if (!base)
		base = "design";

	if (base.length > max)
		base = base.slice(0, max).replace(/[-_]+$/, "");

	return base;
}

function fileName(baseName, slot) {
	const ext = slot.format === "png" ? "png" : "jpg";
	return `${sanitiseBaseName(baseName)}-${slot.id}-${slot.width}x${slot.height}.${ext}`;
}

/* The name a job's file gets, so the caller never has to look the slot up. */
function fileNameForJob(baseName, job) {
	if (!job)
		return "";

	const ext = job.ext === "png" ? "png" : "jpg";
	return `${sanitiseBaseName(baseName)}-${job.slotId}-${job.target.width}x${job.target.height}.${ext}`;
}

function formatBytes(bytes) {
	if (!Number.isFinite(bytes) || bytes < 0)
		return "";

	if (bytes < 1024)
		return `${Math.round(bytes)} B`;
	if (bytes < 1024 * 1024)
		return `${(bytes / 1024).toFixed(0)} KB`;
	return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}

/* The plan for a whole export, in slot order, skipping anything unticked. */
function planExport(docWidth, docHeight, selection, options) {
	const ids = Array.isArray(selection) ? selection : [];
	const opts = Object.assign({}, DEFAULT_OPTIONS, options || {});
	const jobs = [];

	for (const slot of SLOTS) {
		if (!ids.includes(slot.id))
			continue;
		jobs.push(planJob(slot, docWidth, docHeight, opts));
	}

	return jobs;
}

function summarise(results) {
	const list = Array.isArray(results) ? results : [];
	const ok = list.filter((result) => result.ok);
	const failed = list.filter((result) => !result.ok);
	const bytes = ok.reduce((total, result) => total + (Number(result.bytes) || 0), 0);

	return {
		total: list.length,
		ok: ok.length,
		failed: failed.length,
		bytes,
		text:
			failed.length === 0
				? `${ok.length} file${ok.length === 1 ? "" : "s"}${bytes > 0 ? `, ${formatBytes(bytes)}` : ""}`
				: `${ok.length} exported, ${failed.length} failed`,
	};
}

module.exports = {
	SLOTS,
	DEFAULT_SELECTION,
	DEFAULT_OPTIONS,
	getSlot,
	defaultSelection,
	planJob,
	withCropOffset,
	sanitiseBaseName,
	fileName,
	fileNameForJob,
	formatBytes,
	planExport,
	summarise,
	clamp,
};
