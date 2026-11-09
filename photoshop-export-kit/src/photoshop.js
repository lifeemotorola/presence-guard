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
 * Everything that talks to Photoshop, and nothing else.
 *
 * The decisions live in core.js and smartcrop.js where they can be tested. This
 * file only carries them out: duplicate, resize, crop, save, close. Every
 * optional step (sharpening, the detail read used by the smart crop) is allowed
 * to fail without taking the export with it — a plugin that stops halfway
 * through because a filter was unavailable is worse than one that exports
 * something slightly less sharp and says so.
 *
 * The Photoshop DOM is used wherever it has the operation, because a DOM call
 * is far harder to get wrong than a batchPlay descriptor.
 */

"use strict";

const photoshop = require("photoshop");
const { app, core: psCore, action, imaging, constants } = photoshop;
const { storage } = require("uxp");

const logic = require("./core.js");
const smart = require("./smartcrop.js");

const localFileSystem = storage.localFileSystem;

/* The document is read at roughly this many pixels across when looking for the
 * subject: enough to find a face, cheap enough to be instant on a 6000px file. */
const DETAIL_MAP_WIDTH = 96;

const FOLDER_TOKEN_KEY = "exportkit.folderToken";

function cancelled(context, error) {
	return Boolean((context && context.isCancelled) || (error && error.code === -128));
}

/* ------------------------------------------------------------------------- */
/* the document                                                              */
/* ------------------------------------------------------------------------- */

function activeDocumentInfo() {
	const doc = app.activeDocument;
	if (!doc)
		throw new Error("no document is open");

	const width = Math.round(Number(doc.width));
	const height = Math.round(Number(doc.height));
	if (!Number.isFinite(width) || !Number.isFinite(height) || width <= 0 || height <= 0)
		throw new Error("this document has no usable size");

	return { id: doc.id, name: doc.name || "design", width, height };
}

/*
 * Reads the document small and asks smartcrop.js where the subject is.
 * Returns a crop offset in source pixels. If the imaging API is missing or
 * refuses, use the centre; cancellation must propagate instead.
 */
async function analyseForCrop(doc, job, context) {
	const centred = {
		offsetX: Math.round(job.slackX / 2),
		offsetY: Math.round(job.slackY / 2),
		smart: false,
		confidence: 0,
	};

	if (job.mode !== "fill" || (job.slackX === 0 && job.slackY === 0))
		return centred;

	if (!imaging || typeof imaging.getPixels !== "function")
		return centred;

	let imageData = null;

	try {
		const height = Math.max(1, Math.round((DETAIL_MAP_WIDTH * job.source.height) / job.source.width));

		const picked = await imaging.getPixels({
			documentID: doc.id,
			targetSize: { width: DETAIL_MAP_WIDTH, height },
			colorSpace: "RGB",
			componentSize: 8,
		});

		imageData = picked.imageData;
		const data = await imageData.getData({ chunky: true });

		return smart.chooseCrop({
			data,
			mapWidth: imageData.width,
			mapHeight: imageData.height,
			components: imageData.components,
			aspect: job.target.width / job.target.height,
			slackX: job.slackX,
			slackY: job.slackY,
		});
	} catch (error) {
		if (cancelled(context, error))
			throw error;
		return centred;
	} finally {
		try {
			if (imageData && typeof imageData.dispose === "function")
				imageData.dispose();
		} catch (ignored) {
			/* disposing is a courtesy, not a requirement */
		}
	}
}

/* ------------------------------------------------------------------------- */
/* one file                                                                  */
/* ------------------------------------------------------------------------- */

async function padToTarget(doc, target, context) {
	const anchor = constants && constants.AnchorPosition ? constants.AnchorPosition.MIDDLECENTER : undefined;

	try {
		await doc.resizeCanvas(target.width, target.height, anchor);
	} catch (error) {
		if (cancelled(context, error))
			throw error;
		/* Older builds do not accept the anchor; the default is centred too. */
		await doc.resizeCanvas(target.width, target.height);
	}
}

async function sharpen(doc, context) {
	try {
		if (!app.activeDocument || app.activeDocument.id !== doc.id)
			throw new Error("Temporary export document is not active.");
		const result = await action.batchPlay(
			[
				{
					_obj: "unsharpMask",
					_target: [{ _ref: "layer", _enum: "ordinal", _value: "targetEnum" }],
					amount: { _unit: "percentUnit", _value: 55 },
					radius: { _unit: "pixelsUnit", _value: 0.9 },
					threshold: { _unit: "none", _value: 2 },
					_options: { dialogOptions: "dontDisplay" },
				},
			],
			{ synchronousExecution: true },
		);
		for (const item of result || []) {
			if (item && String(item._obj || "").toLowerCase() === "error") {
				const error = new Error(item.message || `Photoshop error ${item.result}`);
				error.code = item.result;
				throw error;
			}
		}
		return true;
	} catch (error) {
		if (cancelled(context, error))
			throw error;
		/* Not every document can take a filter. Export but report the skip. */
		return false;
	}
}

async function saveCopy(doc, file, job, options) {
	if (job.ext === "png") {
		await doc.saveAs.png(file, { compression: 6 }, true);
		return;
	}

	await doc.saveAs.jpg(
		file,
		{
			quality: logic.clamp(Number(options.quality), 0, 12),
			embedColorProfile: true,
		},
		true,
	);
}

/*
 * One slot, start to finish, on a throwaway copy of the document — the
 * source document is not edited. A failure or cancellation can leave an output
 * file behind; the duplicate is closed manually or by modal auto-close.
 */
async function exportOne(doc, job, options, folder, baseName, context) {
	let copy = null;
	let registered = false;
	const control = context && context.hostControl;

	try {
		copy = await doc.duplicate();
		/* After cancellation Photoshop can refuse further changes, including a
		 * manual close. Register the duplicate for cleanup at modal exit. */
		if (control && typeof control.registerAutoCloseDocument === "function") {
			await control.registerAutoCloseDocument(copy.id);
			registered = true;
		}

		await copy.resizeImage(job.resize.width, job.resize.height);

		if (job.mode === "fill") {
			if (job.crop)
				await copy.crop({
					left: job.crop.left,
					top: job.crop.top,
					right: job.crop.right,
					bottom: job.crop.bottom,
				});
		} else {
			await padToTarget(copy, job.target, context);
		}

		const sharpened = options.sharpen ? await sharpen(copy, context) : null;

		const name = logic.fileNameForJob(baseName, job);
		const file = await folder.createFile(name, { overwrite: true });
		await saveCopy(copy, file, job, options);

		let bytes = null;
		try {
			const size = Number(file.size);
			if (Number.isFinite(size) && size > 0)
				bytes = size;
		} catch (ignored) {
			/* the file is on disk whether or not it reports a size */
		}

		return {
			ok: true, name, bytes, sharpened,
			smart: Boolean(options.smartCrop && job.smartCrop === true),
			cropDecision: job.smartCrop === true ? "detail crop" : job.smartCrop === false ? "centered crop" : null,
		};
	} finally {
		if (copy) {
			let closed = false;
			try {
				await copy.closeWithoutSaving();
				closed = true;
			} catch (error) {
				if (!registered && !cancelled(context, error))
					throw new Error("The temporary export document could not be closed; close it without saving.");
				/* Registered copies will be closed by the host at modal exit. */
			}
			if (closed && registered && typeof control.unregisterAutoCloseDocument === "function") {
				try { await control.unregisterAutoCloseDocument(copy.id); } catch (ignored) { /* already closed */ }
			}
		}
	}
}

/* ------------------------------------------------------------------------- */
/* the export                                                                */
/* ------------------------------------------------------------------------- */

/*
 * Runs a whole plan. Each slot is isolated: one failure is reported and the
 * rest still export, because "seven of eight worked" beats "nothing happened".
 */
async function exportJobs(jobs, options) {
	const { folder, settings, baseName, onProgress, documentId } = options;
	const results = [];
	let wasCancelled = false;

	if (!folder)
		throw new Error("choose a folder first");
	if (!jobs || jobs.length === 0)
		return { results, cancelled: false };

	await psCore.executeAsModal(
		async (executionContext) => {
			const doc = app.activeDocument;
			if (!doc)
				throw new Error("no document is open");
			if (documentId !== undefined && doc.id !== documentId)
				throw new Error("The active document changed. Check the document and export again.");

			for (let index = 0; index < jobs.length; index++) {
				if (cancelled(executionContext)) {
					wasCancelled = true;
					break;
				}
				const planned = jobs[index];
				if (typeof onProgress === "function")
					onProgress({ index, total: jobs.length, slotId: planned.slotId, label: planned.label });

				try {
					let job = planned;
					if (settings.smartCrop && planned.mode === "fill" && (planned.slackX > 0 || planned.slackY > 0)) {
						const where = await analyseForCrop(doc, planned, executionContext);
						job = logic.withCropOffset(planned, where.offsetX, where.offsetY);
						job.smartCrop = Boolean(where.smart);
					}
					if (cancelled(executionContext)) {
						wasCancelled = true;
						break;
					}
					const result = await exportOne(doc, job, settings, folder, baseName, executionContext);
					results.push(Object.assign({ slotId: planned.slotId, label: planned.label }, result));
				} catch (error) {
					if (cancelled(executionContext, error)) {
						wasCancelled = true;
						break;
					}
					results.push({
						ok: false,
						slotId: planned.slotId,
						label: planned.label,
						error: String((error && error.message) || error),
					});
				}
			}
			if (cancelled(executionContext))
				wasCancelled = true;
		},
		{ commandName: "Export Kit" },
	);
	return { results, cancelled: wasCancelled };
}

/* ------------------------------------------------------------------------- */
/* the destination folder                                                    */
/* ------------------------------------------------------------------------- */

async function chooseFolder() {
	const folder = await localFileSystem.getFolder();
	if (!folder)
		return null;

	try {
		const token = await localFileSystem.createPersistentToken(folder);
		window.localStorage.setItem(FOLDER_TOKEN_KEY, token);
	} catch (ignored) {
		/* remembering the folder is a convenience, not a requirement */
	}

	return folder;
}

/* The folder the user picked last time, if it can be reached again. */
async function rememberedFolder() {
	let token = null;
	try {
		token = window.localStorage.getItem(FOLDER_TOKEN_KEY);
	} catch (ignored) {
		return null;
	}

	if (!token)
		return null;

	try {
		return await localFileSystem.getEntryForPersistentToken(token);
	} catch (ignored) {
		return null;
	}
}

module.exports = {
	activeDocumentInfo,
	analyseForCrop,
	exportOne,
	exportJobs,
	chooseFolder,
	rememberedFolder,
	DETAIL_MAP_WIDTH,
};
