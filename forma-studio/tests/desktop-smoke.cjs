/* Forma Studio packaged desktop test. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const { _electron: electron } = require('playwright-core');

const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
async function eventually(check, label, timeout = 30000) {
  const until = Date.now() + timeout;
  let last;
  while (Date.now() < until) {
    try { last = await check(); if (last) return last; } catch { /* file may not exist yet */ }
    await wait(100);
  }
  throw new Error(`Timed out waiting for ${label}`);
}
async function commit(locator, value) { await locator.fill(String(value)); await locator.press('Tab'); }
async function readProject(file, predicate = () => true) {
  return eventually(async () => { const data = JSON.parse(await fs.readFile(file, 'utf8')); return predicate(data) ? data : null; }, path.basename(file));
}
async function waitStatus(page, text) {
  await page.waitForFunction((part) => document.querySelector('#status-text').textContent.includes(part), text, { timeout: 30000 });
}
async function installDialogSelections(desktop) {
  // Only the operating-system picker responses are stubbed. IPC, validation,
  // renderer state, atomic disk writes and exports use the packaged app.
  await desktop.evaluate(({ dialog }) => {
    const state = { savePath: null, openPath: null, confirm: 0, saves: [], opens: [], warnings: [] };
    globalThis.__formaTestDialogs = state;
    dialog.showSaveDialog = async (_window, options) => {
      state.saves.push({ title: options.title, defaultPath: options.defaultPath });
      const filePath = state.savePath; state.savePath = null;
      return filePath ? { canceled: false, filePath } : { canceled: true };
    };
    dialog.showOpenDialog = async (_window, options) => {
      state.opens.push({ title: options.title });
      const filePath = state.openPath; state.openPath = null;
      return filePath ? { canceled: false, filePaths: [filePath] } : { canceled: true, filePaths: [] };
    };
    dialog.showMessageBoxSync = (_window, options) => {
      state.warnings.push({ title: options.title, message: options.message });
      return state.confirm;
    };
  });
}
async function choose(desktop, key, value) {
  await desktop.evaluate((_electron, data) => { globalThis.__formaTestDialogs[data.key] = data.value; }, { key, value });
}
async function menuAction(page, button) {
  await page.locator('#file-menu summary').click();
  await page.locator(button).click();
}
async function exportAction(page, button) {
  await page.locator('.export-menu summary').click();
  await page.locator(button).click();
}
async function launch(exe, env) {
  const desktop = await electron.launch({ executablePath: exe, env, timeout: 120000 });
  const page = await desktop.firstWindow({ timeout: 120000 });
  page.setDefaultTimeout(30000);
  const errors = [];
  page.on('pageerror', (error) => errors.push(error.message));
  await page.waitForLoadState('domcontentloaded');
  await page.locator('#viewport-canvas canvas').waitFor({ timeout: 60000 });
  assert.equal(await page.locator('#webgl-error').isVisible(), false, 'packaged app needs a working WebGL viewport');
  await installDialogSelections(desktop);
  return { desktop, page, errors };
}

(async () => {
  const exe = path.resolve(process.env.FORMA_EXE || path.join(__dirname, '..', 'out', 'FormaStudio-win32-x64', 'FormaStudio.exe'));
  await fs.access(exe);
  const temp = await fs.mkdtemp(path.join(os.tmpdir(), 'forma-studio-test-'));
  const env = { ...process.env, FORMA_TEST_DATA_DIR: path.join(temp, 'profile'), FORMA_TEST_SOFTWARE_GL: '1' };
  const sceneA = path.join(temp, 'scene A.forma'), sceneB = path.join(temp, 'scene B.forma');
  const invalid = path.join(temp, 'invalid.forma'), large = path.join(temp, 'large.forma');
  const glb = path.join(temp, 'snapshot.glb'), png = path.join(temp, 'viewport.png');
  let desktop;
  try {
    let result = await launch(exe, env);
    ({ desktop } = result);
    let { page, errors } = result;
    const preferences = await desktop.evaluate(({ BrowserWindow }) => {
      const prefs = BrowserWindow.getAllWindows()[0].webContents.getLastWebPreferences();
      return { sandbox: prefs.sandbox, contextIsolation: prefs.contextIsolation, nodeIntegration: prefs.nodeIntegration, webSecurity: prefs.webSecurity };
    });
    assert.deepEqual(preferences, { sandbox: true, contextIsolation: true, nodeIntegration: false, webSecurity: true });
    assert.deepEqual(await page.evaluate(() => ({ require: typeof window.require, process: typeof window.process, bridge: typeof window.forma })),
      { require: 'undefined', process: 'undefined', bridge: 'object' });
    assert.equal(await page.evaluate(async () => { try { await fetch('https://github.com/'); return false; } catch { return true; } }), true, 'renderer must be offline');
    if (process.env.FORMA_SCREENSHOT) await page.screenshot({ path: process.env.FORMA_SCREENSHOT });

    // Build a scene, with array and actual keyframe interpolation.
    await page.getByRole('button', { name: 'Cube', exact: true }).click();
    await page.getByRole('checkbox', { name: 'Enable procedural array' }).check();
    await page.locator('#array-mode').selectOption('radial');
    await commit(page.locator('#array-count'), 4);
    await page.getByRole('button', { name: 'Add keyframe' }).click();
    await commit(page.getByRole('spinbutton', { name: 'Current frame' }), 24);
    await commit(page.getByRole('spinbutton', { name: 'Position X' }), 6);
    await commit(page.getByRole('spinbutton', { name: 'Current frame' }), 12);
    assert.equal(await page.getByRole('spinbutton', { name: 'Position X' }).inputValue(), '3');
    if (process.env.FORMA_FEATURE_SCREENSHOT) await page.screenshot({ path: process.env.FORMA_FEATURE_SCREENSHOT });

    // Save, then overwrite the same file via the native bridge (atomic rename).
    await choose(desktop, 'savePath', sceneA);
    await page.locator('#save-project').click();
    let project = await readProject(sceneA, (value) => value.objects.length === 5);
    let cube = project.objects.find((object) => object.name === 'Cube 01');
    assert.deepEqual(cube.keyframes.map((key) => key.frame), [0, 24]);
    assert.deepEqual({ mode: cube.array.mode, count: cube.array.count }, { mode: 'radial', count: 4 });
    await commit(page.locator('#object-name'), 'Saved cube A');
    await page.locator('#save-project').click();
    project = await readProject(sceneA, (value) => value.objects.some((object) => object.name === 'Saved cube A'));
    assert.equal((await fs.readdir(temp)).filter((name) => name.endsWith('.tmp')).length, 0, 'temporary saves are cleaned up');
    let dialogs = await desktop.evaluate(() => globalThis.__formaTestDialogs);
    assert.equal(dialogs.saves.length, 1, 'ordinary Save reuses the current project path');

    // Save As switches the project path. A rejected or oversized Open must not
    // replace either the scene or the current save destination.
    await choose(desktop, 'savePath', sceneB);
    await menuAction(page, '#save-as');
    await readProject(sceneB, (value) => value.objects.some((object) => object.name === 'Saved cube A'));
    await fs.writeFile(invalid, '{"format":"not-forma"}', 'utf8');
    await choose(desktop, 'openPath', invalid);
    await menuAction(page, '#open-project');
    await waitStatus(page, 'Could not open project');
    assert.equal(await page.locator('.object-row[role=listitem]').count(), 5);
    await fs.writeFile(large, Buffer.alloc(10 * 1024 * 1024 + 1, 32));
    await choose(desktop, 'openPath', large);
    await menuAction(page, '#open-project');
    await waitStatus(page, '10 MB or smaller');
    await commit(page.locator('#object-name'), 'Saved cube B');
    await page.locator('#save-project').click();
    await readProject(sceneB, (value) => value.objects.some((object) => object.name === 'Saved cube B'));
    assert.equal(await fs.readFile(invalid, 'utf8'), '{"format":"not-forma"}', 'invalid input is not overwritten');
    assert.ok((await readProject(sceneA)).objects.some((object) => object.name === 'Saved cube A'));

    // Open accepts the validated file as the new save destination.
    await choose(desktop, 'openPath', sceneA);
    await menuAction(page, '#open-project');
    await waitStatus(page, 'Project opened');
    await page.getByRole('button', { name: 'Select Saved cube A' }).click();
    await commit(page.getByRole('spinbutton', { name: 'Current frame' }), 12);
    assert.equal(await page.getByRole('spinbutton', { name: 'Position X' }).inputValue(), '3');

    await choose(desktop, 'savePath', glb);
    await exportAction(page, '[data-export=glb]');
    await waitStatus(page, 'Exported the current frame');
    const glbBytes = await fs.readFile(glb);
    assert.equal(glbBytes.toString('ascii', 0, 4), 'glTF');
    assert.equal(glbBytes.readUInt32LE(4), 2);
    assert.equal(glbBytes.readUInt32LE(8), glbBytes.length);
    const jsonLength = glbBytes.readUInt32LE(12);
    const gltf = JSON.parse(glbBytes.subarray(20, 20 + jsonLength).toString('utf8').trim());
    assert.ok(gltf.nodes.filter((node) => Number.isInteger(node.mesh)).length >= 8, 'array copies must be baked into GLB');
    assert.equal(gltf.nodes.find((node) => node.name === 'Saved cube A').translation[0], 3, 'GLB must reflect the interpolated current frame');
    assert.equal(gltf.animations, undefined, 'GLB is a current-frame snapshot, not animation');
    await choose(desktop, 'savePath', png);
    await exportAction(page, '[data-export=png]');
    await waitStatus(page, 'Saved the realtime viewport');
    const pngBytes = await fs.readFile(png);
    assert.equal(pngBytes.subarray(0, 8).toString('hex'), '89504e470d0a1a0a');
    assert.ok(pngBytes.readUInt32BE(16) > 200 && pngBytes.readUInt32BE(20) > 100);
    assert.deepEqual(errors, []);
    await desktop.close(); desktop = null;

    // Reopen a real saved scene after quitting the packaged app.
    result = await launch(exe, env);
    ({ desktop, page, errors } = result);
    await choose(desktop, 'openPath', sceneA);
    await menuAction(page, '#open-project');
    await waitStatus(page, 'Project opened');
    await page.getByRole('button', { name: 'Select Saved cube A' }).click();
    await commit(page.getByRole('spinbutton', { name: 'Current frame' }), 12);
    assert.equal(await page.getByRole('spinbutton', { name: 'Position X' }).inputValue(), '3');

    // Closing a dirty window invokes the host's warning. Cancel preserves the
    // scene; discard closes the real BrowserWindow.
    await page.getByRole('button', { name: 'Sphere', exact: true }).click();
    await page.evaluate(() => window.close());
    await eventually(async () => (await desktop.evaluate(() => globalThis.__formaTestDialogs.warnings.length)) > 0, 'unsaved-close warning');
    assert.equal(page.isClosed(), false, 'Cancel must keep the dirty window open');
    assert.equal(await page.locator('.object-row[role=listitem]').count(), 6);
    await choose(desktop, 'confirm', 1);
    const closed = page.waitForEvent('close', { timeout: 30000 });
    await page.evaluate(() => window.close()).catch(() => {});
    await closed;
    assert.deepEqual(errors, []);
    await desktop.close().catch(() => {});
    desktop = null;
    console.log('Packaged Windows app: sandbox, offline renderer, editing, arrays, animation, save/overwrite/open, size limits, GLB/PNG, restart and unsaved-close protection passed.');
  } finally {
    if (desktop) {
      await choose(desktop, 'confirm', 1).catch(() => {});
      await desktop.close().catch(() => {});
    }
    await fs.rm(temp, { recursive: true, force: true, maxRetries: 10, retryDelay: 500 });
  }
})().catch((error) => { console.error(error); process.exitCode = 1; });
