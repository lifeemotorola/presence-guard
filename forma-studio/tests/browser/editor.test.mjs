/* Forma Studio browser tests. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
import test, { before, after } from 'node:test';
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { dirname, extname, resolve, sep } from 'node:path';
import { fileURLToPath } from 'node:url';
import { chromium } from 'playwright-core';
import AxeBuilder from '@axe-core/playwright';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../../app');
const mime = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.svg': 'image/svg+xml', '.png': 'image/png', '.woff2': 'font/woff2' };
let server, browser, base;
before(async () => {
  server = createServer(async (req, res) => {
    const pathname = new URL(req.url, 'http://test.local').pathname;
    const file = resolve(root, `.${pathname === '/' ? '/index.html' : pathname}`);
    if (!file.startsWith(root + sep)) { res.writeHead(403).end(); return; }
    try { const data = await readFile(file); res.writeHead(200, { 'Content-Type': mime[extname(file)] || 'application/octet-stream' }); res.end(data); }
    catch { res.writeHead(404).end(); }
  });
  await new Promise((resolveListen) => server.listen(0, '0.0.0.0', resolveListen));
  base = `http://127.0.0.1:${server.address().port}`;
  browser = await chromium.launch({ executablePath: process.env.CHROMIUM_PATH || '/usr/bin/google-chrome', headless: true, args: ['--no-sandbox', '--disable-dev-shm-usage', '--enable-unsafe-swiftshader', '--use-angle=swiftshader'] });
});
after(async () => { await browser?.close(); if (server) await new Promise((done) => server.close(done)); });

async function openPage(width = 1500, height = 940) {
  const context = await browser.newContext({ viewport: { width, height }, acceptDownloads: true });
  const page = await context.newPage(); const errors = [];
  page.on('pageerror', (error) => errors.push(error.message));
  await page.goto(base, { waitUntil: 'domcontentloaded' });
  await page.locator('#viewport-canvas canvas').waitFor({ timeout: 30000 });
  assert.equal(await page.locator('#webgl-error').isVisible(), false, 'WebGL must initialize');
  return { page, context, errors };
}
async function commitInput(locator, value) { await locator.fill(String(value)); await locator.press('Tab'); }
async function downloadFrom(page, action) {
  const promise = page.waitForEvent('download'); await action(); const download = await promise;
  return { download, bytes: await readFile(await download.path()) };
}

test('scene editing, procedural array, materials, undo/redo and visibility', async () => {
  const { page, context, errors } = await openPage();
  assert.equal(await page.locator('.object-row[role=listitem]').count(), 4);
  await page.getByRole('button', { name: 'Cube', exact: true }).click();
  assert.equal(await page.locator('.object-row[role=listitem]').count(), 5);
  assert.equal(await page.locator('#selected-name').textContent(), 'Cube 01');
  await commitInput(page.getByRole('spinbutton', { name: 'Position X' }), 2.5);
  assert.equal(await page.getByRole('spinbutton', { name: 'Position X' }).inputValue(), '2.5');
  await page.getByRole('button', { name: 'Undo', exact: true }).click();
  assert.equal(await page.getByRole('spinbutton', { name: 'Position X' }).inputValue(), '0');
  await page.getByRole('button', { name: 'Redo', exact: true }).click();
  assert.equal(await page.getByRole('spinbutton', { name: 'Position X' }).inputValue(), '2.5');
  await page.getByRole('checkbox', { name: 'Enable procedural array' }).check();
  await page.locator('#array-mode').selectOption('radial');
  await commitInput(page.locator('#array-count'), 6);
  await commitInput(page.locator('#array-distance'), 2.5);
  await page.getByRole('tab', { name: 'Material' }).click();
  await page.locator('#material-color').fill('#35cc88');
  await page.locator('#roughness').fill('0.3');
  await page.locator('#roughness').dispatchEvent('change');
  await page.getByRole('tab', { name: 'Object' }).click();
  await page.getByRole('button', { name: 'Duplicate', exact: true }).click();
  assert.equal(await page.locator('.object-row[role=listitem]').count(), 6);
  await page.getByRole('button', { name: 'Hide Cube 01 copy' }).click();
  assert.equal(await page.getByRole('button', { name: 'Show Cube 01 copy' }).count(), 1);
  await page.getByRole('button', { name: 'Delete', exact: true }).click();
  assert.equal(await page.locator('.object-row[role=listitem]').count(), 5);
  await page.getByRole('button', { name: 'Undo', exact: true }).click();
  assert.equal(await page.locator('.object-row[role=listitem]').count(), 6);
  const { bytes } = await downloadFrom(page, () => page.getByRole('button', { name: 'Save project', exact: true }).click());
  const project = JSON.parse(bytes.toString('utf8'));
  const cube = project.objects.find((object) => object.name === 'Cube 01');
  assert.deepEqual(cube.array, { enabled: true, mode: 'radial', count: 6, spacing: 1.8, radius: 2.5 });
  assert.equal(cube.material.color, '#35cc88');
  assert.equal(cube.material.roughness, 0.3);
  assert.equal(project.objects.find((object) => object.name === 'Cube 01 copy').visible, false);
  assert.deepEqual(errors, []);
  await context.close();
});

test('dragging a real viewport gizmo changes the scene and can be undone', async () => {
  const { page, context, errors } = await openPage();
  const position = page.getByRole('spinbutton', { name: 'Position X' });
  const initial = Number(await position.inputValue());
  const canvas = await page.locator('#viewport-canvas canvas').boundingBox();
  assert.ok(canvas);
  // The starter object's red X handle, projected in the fixed-size viewport.
  const x = canvas.x + canvas.width * 0.52, y = canvas.y + canvas.height * 0.482;
  await page.mouse.move(x, y); await page.mouse.down(); await page.mouse.move(x + 52, y + 19, { steps: 12 }); await page.mouse.up();
  assert.notEqual(Number(await position.inputValue()), initial, 'dragging the actual transform handle must move the object');
  await page.getByRole('button', { name: 'Undo', exact: true }).click();
  assert.equal(Number(await position.inputValue()), initial);
  assert.deepEqual(errors, []);
  await context.close();
});

test('transform keyframes interpolate, auto-key edits and survive project saving', async () => {
  const { page, context, errors } = await openPage();
  await page.getByRole('button', { name: 'Cube', exact: true }).click();
  await page.getByRole('button', { name: 'Add keyframe' }).click();
  await commitInput(page.getByRole('spinbutton', { name: 'Current frame' }), 24);
  await commitInput(page.getByRole('spinbutton', { name: 'Position X' }), 6);
  assert.equal(await page.locator('.key-marker').count(), 2);
  await commitInput(page.getByRole('spinbutton', { name: 'Current frame' }), 12);
  assert.equal(await page.getByRole('spinbutton', { name: 'Position X' }).inputValue(), '3');
  await page.getByRole('button', { name: 'Play animation' }).click();
  await page.waitForTimeout(350);
  await page.getByRole('button', { name: 'Pause animation' }).click();
  assert.notEqual(await page.getByRole('spinbutton', { name: 'Current frame' }).inputValue(), '12');
  const { bytes } = await downloadFrom(page, () => page.getByRole('button', { name: 'Save project', exact: true }).click());
  const project = JSON.parse(bytes.toString('utf8'));
  const cube = project.objects.find((object) => object.name === 'Cube 01');
  assert.deepEqual(cube.keyframes.map((key) => key.frame), [0, 24]);
  assert.equal(cube.keyframes[1].transform.position[0], 6);
  assert.deepEqual(errors, []);
  await context.close();
});

test('save/open protects unsaved work and rejects malformed projects without replacing the scene', async () => {
  const { page, context, errors } = await openPage();
  await page.getByRole('button', { name: 'Sphere', exact: true }).click();
  const { bytes } = await downloadFrom(page, () => page.getByRole('button', { name: 'Save project', exact: true }).click());
  await page.getByRole('button', { name: 'Cone', exact: true }).click();
  await page.locator('#file-menu summary').click(); await page.getByRole('button', { name: /New scene/ }).click();
  assert.equal(await page.locator('#confirm-dialog').isVisible(), true);
  await page.getByRole('button', { name: 'Cancel', exact: true }).click();
  assert.equal(await page.locator('.object-row[role=listitem]').count(), 6);
  await page.locator('#file-menu summary').click(); await page.getByRole('button', { name: /New scene/ }).click();
  await page.getByRole('button', { name: 'Discard', exact: true }).click();
  assert.equal(await page.locator('.object-row[role=listitem]').count(), 0);
  const choose = page.waitForEvent('filechooser');
  await page.locator('#file-menu summary').click(); await page.getByRole('button', { name: /Open project/ }).click();
  const chooser = await choose;
  await chooser.setFiles({ name: 'saved.forma', mimeType: 'application/json', buffer: bytes });
  await page.waitForFunction(() => document.querySelectorAll('.object-row[role=listitem]').length === 5);
  const invalid = page.waitForEvent('filechooser');
  await page.locator('#file-menu summary').click(); await page.getByRole('button', { name: /Open project/ }).click();
  await (await invalid).setFiles({ name: 'bad.forma', mimeType: 'application/json', buffer: Buffer.from('{"format":"bad"}') });
  await page.waitForFunction(() => document.querySelector('#status-text').textContent.includes('Could not open project'));
  assert.equal(await page.locator('.object-row[role=listitem]').count(), 5);
  assert.deepEqual(errors, []);
  await context.close();
});

test('saving from a focused field commits its value before serializing', async () => {
  const { page, context, errors } = await openPage();
  await page.locator('#object-name').fill('A newly named object');
  const { bytes } = await downloadFrom(page, () => page.keyboard.press('Control+s'));
  const project = JSON.parse(bytes.toString('utf8'));
  assert.equal(project.objects.find((object) => object.id === 'starter-form').name, 'A newly named object');
  assert.equal(await page.locator('#dirty-indicator').isVisible(), false);
  assert.deepEqual(errors, []);
  await context.close();
});

test('edits made during a pending save remain dirty and are not discarded by New', async () => {
  const { page, context, errors } = await openPage();
  await page.evaluate(() => {
    window.forma = {
      saveProject: (text) => new Promise((resolve) => { window.pendingSave = { text, resolve }; }),
      forgetProject: async () => {},
    };
  });
  await page.getByRole('button', { name: 'Cube', exact: true }).click();
  await page.locator('#file-menu summary').click();
  await page.getByRole('button', { name: /New scene/ }).click();
  await page.locator('#confirm-dialog').getByRole('button', { name: 'Save project' }).click();
  await page.waitForFunction(() => Boolean(window.pendingSave));
  const snapshot = JSON.parse(await page.evaluate(() => window.pendingSave.text));
  assert.equal(snapshot.objects.length, 5);
  await page.getByRole('button', { name: 'Sphere', exact: true }).click();
  await page.evaluate(() => window.pendingSave.resolve({ canceled: false }));
  await page.waitForFunction(() => document.querySelector('#status-text').textContent.includes('newer changes'));
  assert.equal(await page.locator('.object-row[role=listitem]').count(), 6);
  assert.equal(await page.locator('#dirty-indicator').isVisible(), true);
  assert.deepEqual(errors, []);
  await context.close();
});

test('GLB and PNG exports are real files and GLB contains baked visible meshes', async () => {
  const { page, context, errors } = await openPage();
  await page.getByRole('button', { name: 'Cube', exact: true }).click();
  await page.getByRole('checkbox', { name: 'Enable procedural array' }).check();
  await commitInput(page.locator('#array-count'), 4);
  await page.getByRole('button', { name: 'Add keyframe' }).click();
  await commitInput(page.getByRole('spinbutton', { name: 'Current frame' }), 24);
  await commitInput(page.getByRole('spinbutton', { name: 'Position X' }), 6);
  await commitInput(page.getByRole('spinbutton', { name: 'Current frame' }), 12);
  await page.getByRole('button', { name: 'Hide Mint sphere' }).click();
  const glb = await downloadFrom(page, async () => { await page.locator('.export-menu summary').click(); await page.getByRole('button', { name: /3D model/ }).click(); });
  assert.equal(glb.bytes.toString('ascii', 0, 4), 'glTF');
  assert.equal(glb.bytes.readUInt32LE(4), 2);
  assert.equal(glb.bytes.readUInt32LE(8), glb.bytes.length);
  const jsonLength = glb.bytes.readUInt32LE(12);
  assert.equal(glb.bytes.toString('ascii', 16, 20), 'JSON');
  const gltf = JSON.parse(glb.bytes.subarray(20, 20 + jsonLength).toString('utf8').trim());
  assert.equal(gltf.nodes.filter((node) => Number.isInteger(node.mesh)).length, 7, 'only visible objects, including four array copies, are baked into the GLB');
  assert.equal(gltf.nodes.find((node) => node.name === 'Cube 01').translation[0], 3, 'export reflects the interpolated current frame');
  assert.equal(gltf.nodes.some((node) => node.name === 'Mint sphere'), false, 'hidden objects are not exported');
  assert.equal(gltf.animations, undefined, 'the export is explicitly a current-frame snapshot');
  const png = await downloadFrom(page, async () => { await page.locator('.export-menu summary').click(); await page.getByRole('button', { name: /Viewport image/ }).click(); });
  assert.equal(png.bytes.subarray(0, 8).toString('hex'), '89504e470d0a1a0a');
  assert.ok(png.bytes.readUInt32BE(16) > 200 && png.bytes.readUInt32BE(20) > 100);
  assert.deepEqual(errors, []);
  await context.close();
});

test('editor panels and help dialog have no automated accessibility violations', async () => {
  const { page, context } = await openPage();
  for (const state of ['Object', 'Material', 'Scene', 'Help']) {
    if (state === 'Help') await page.getByRole('button', { name: 'Help and shortcuts' }).click();
    else await page.getByRole('tab', { name: state, exact: true }).click();
    const { violations } = await new AxeBuilder({ page }).analyze();
    assert.deepEqual(violations.map(({ id, nodes }) => `${id}: ${nodes.map((node) => node.target.join(' ')).join(', ')}`), [], state);
  }
  await context.close();
});

test('desktop, narrow window and small browser layouts remain usable without horizontal clipping', async () => {
  for (const [width, height] of [[1500, 940], [1000, 700], [800, 900], [375, 812]]) {
    const { page, context, errors } = await openPage(width, height);
    assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1), true, `horizontal overflow at ${width}`);
    assert.equal(await page.getByRole('button', { name: 'Cube', exact: true }).count(), 1);
    await page.getByRole('button', { name: 'Cube', exact: true }).click();
    assert.equal(await page.getByRole('spinbutton', { name: 'Position X' }).count(), 1);
    assert.deepEqual(errors, []);
    await context.close();
  }
});
