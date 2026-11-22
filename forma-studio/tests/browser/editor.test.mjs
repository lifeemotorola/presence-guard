/* Forma Studio browser tests. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
import test, { before, after } from 'node:test';
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { dirname, extname, resolve, sep } from 'node:path';
import { fileURLToPath } from 'node:url';
import { chromium } from 'playwright-core';
import AxeBuilder from '@axe-core/playwright';
import { createStarterProject, createObject } from '../../app/model.js';

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
async function nextRender(page) {
  await page.evaluate(() => new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve))));
}
async function colorRegions(page) {
  await nextRender(page);
  return page.evaluate(() => {
    const canvas = document.querySelector('#viewport-canvas canvas');
    const copy = document.createElement('canvas'); copy.width = canvas.width; copy.height = canvas.height;
    const ctx = copy.getContext('2d'); ctx.drawImage(canvas, 0, 0);
    const { data, width, height } = ctx.getImageData(0, 0, copy.width, copy.height);
    const regions = { red: null, green: null };
    for (let y = 0; y < height; y++) for (let x = 0; x < width; x++) {
      const offset = (y * width + x) * 4, r = data[offset], g = data[offset + 1], b = data[offset + 2];
      const color = r > 70 && r > g * 1.5 && r > b * 1.5 ? 'red' : g > 70 && g > r * 1.5 && g > b * 1.5 ? 'green' : null;
      if (!color) continue;
      const region = regions[color] ||= { minX: x, maxX: x, minY: y, maxY: y };
      region.minX = Math.min(region.minX, x); region.maxX = Math.max(region.maxX, x);
      region.minY = Math.min(region.minY, y); region.maxY = Math.max(region.maxY, y);
    }
    return { width, height, ...regions };
  });
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

test('opening an older project requests a new version-2 save path and preserves that requirement after cancel', async () => {
  const { page, context, errors } = await openPage();
  const legacy = createStarterProject(); legacy.version = 1;
  await page.evaluate((text) => {
    window.legacySaves = [];
    window.forma = {
      openProject: async () => ({ canceled: false, text }),
      acceptOpenedProject: async () => {},
      saveProject: async (contents, name, saveAs) => {
        window.legacySaves.push({ contents, name, saveAs });
        return { canceled: window.legacySaves.length === 1 };
      },
    };
  }, JSON.stringify(legacy));
  await page.locator('#file-menu summary').click(); await page.locator('#open-project').click();
  await page.waitForFunction(() => document.querySelector('#status-text').textContent.includes('first save'));
  await page.locator('#save-project').click();
  await page.waitForFunction(() => window.legacySaves.length === 1);
  await page.locator('#save-project').click();
  await page.waitForFunction(() => window.legacySaves.length === 2 && document.querySelector('#status-text').textContent === 'Project saved.');
  let calls = await page.evaluate(() => window.legacySaves);
  assert.ok(calls.slice(0, 2).every((call) => call.saveAs && call.name.endsWith('-v2')));
  assert.equal(JSON.parse(calls[1].contents).version, 2);
  await page.getByRole('button', { name: 'Cube', exact: true }).click();
  await page.locator('#save-project').click();
  await page.waitForFunction(() => window.legacySaves.length === 3);
  calls = await page.evaluate(() => window.legacySaves);
  assert.equal(calls[2].saveAs, false, 'subsequent saves reuse the new version-2 path');
  assert.deepEqual(errors, []);
  await context.close();
});

test('mesh editing modifies topology, persists shading and arrays, and exports a clean current frame', async () => {
  const { page, context, errors } = await openPage();
  await page.locator('#file-menu summary').click(); await page.locator('#new-project').click();
  await page.getByRole('button', { name: 'Cube', exact: true }).click();
  await page.getByRole('button', { name: 'Frame selected object' }).click();
  await page.locator('#view-select').selectOption('front');
  await page.getByRole('button', { name: 'Model', exact: true }).click();
  await page.getByRole('button', { name: 'Edit mesh', exact: true }).click();
  assert.equal(await page.locator('#mesh-count').textContent(), '8 vertices · 6 faces');
  assert.equal(await page.getByRole('button', { name: 'Move tool' }).isDisabled(), true);
  const canvas = await page.locator('#viewport-canvas canvas').boundingBox();
  await page.mouse.click(canvas.x + canvas.width / 2, canvas.y + canvas.height / 2);
  assert.equal(await page.locator('#mesh-component-number').inputValue(), '2', 'the visible front face is picked by raycasting');
  await commitInput(page.locator('#extrude-distance'), 0);
  await page.getByRole('button', { name: 'Extrude face' }).click();
  assert.match(await page.locator('#status-text').textContent(), /distance/);
  assert.equal(await page.locator('#mesh-count').textContent(), '8 vertices · 6 faces');
  await commitInput(page.locator('#extrude-distance'), 1);
  await page.keyboard.press('e');
  assert.equal(await page.locator('#mesh-count').textContent(), '12 vertices · 10 faces');
  await page.keyboard.press('i');
  assert.equal(await page.locator('#mesh-count').textContent(), '16 vertices · 14 faces');
  await page.getByRole('button', { name: 'Undo', exact: true }).click();
  assert.equal(await page.locator('#mesh-count').textContent(), '12 vertices · 10 faces');
  await page.getByRole('button', { name: 'Redo', exact: true }).click();
  assert.equal(await page.locator('#mesh-count').textContent(), '16 vertices · 14 faces');
  await page.getByRole('button', { name: 'Subdivide mesh' }).click();
  assert.equal(await page.locator('#mesh-count').textContent(), '58 vertices · 56 faces');
  await page.getByRole('button', { name: 'Undo', exact: true }).click();
  assert.equal(await page.locator('#mesh-count').textContent(), '16 vertices · 14 faces');
  await page.keyboard.press('1');
  assert.equal(await page.getByRole('button', { name: 'Vertex', exact: true }).getAttribute('aria-pressed'), 'true');
  await commitInput(page.getByRole('spinbutton', { name: 'Vertex number' }), 7);
  await commitInput(page.getByRole('spinbutton', { name: 'Vertex X' }), 1);
  await page.getByRole('checkbox', { name: 'Smooth mesh shading' }).check();
  await page.getByRole('tab', { name: 'Object', exact: true }).click();
  await page.getByRole('checkbox', { name: 'Enable procedural array' }).check();
  await commitInput(page.locator('#array-count'), 3);
  await page.getByRole('button', { name: 'Add keyframe' }).click();
  await commitInput(page.getByRole('spinbutton', { name: 'Current frame' }), 24);
  await commitInput(page.getByRole('spinbutton', { name: 'Position X' }), 4);
  await commitInput(page.getByRole('spinbutton', { name: 'Current frame' }), 12);
  assert.equal(await page.getByRole('spinbutton', { name: 'Position X' }).inputValue(), '2');
  await page.getByRole('tab', { name: 'Model', exact: true }).click();
  assert.equal(await page.locator('#mesh-count').textContent(), '16 vertices · 14 faces');
  const saved = await downloadFrom(page, () => page.getByRole('button', { name: 'Save project', exact: true }).click());
  const project = JSON.parse(saved.bytes.toString('utf8'));
  assert.equal(project.version, 2);
  const cube = project.objects[0];
  assert.equal(cube.type, 'mesh'); assert.equal(cube.shading, 'smooth');
  assert.equal(cube.mesh.vertices.length, 16); assert.equal(cube.mesh.faces.length, 14);
  assert.equal(cube.mesh.vertices[6][0], 1);
  assert.equal(cube.array.count, 3);
  assert.deepEqual(cube.keyframes.map((key) => key.frame), [0, 24]);
  const glb = await downloadFrom(page, async () => { await page.locator('.export-menu summary').click(); await page.locator('.export-menu [data-export=glb]').click(); });
  assert.equal(glb.bytes.toString('ascii', 0, 4), 'glTF');
  const jsonLength = glb.bytes.readUInt32LE(12);
  const gltf = JSON.parse(glb.bytes.subarray(20, 20 + jsonLength).toString('utf8').trim());
  assert.equal(gltf.nodes.filter((node) => Number.isInteger(node.mesh)).length, 3, 'array copies are exported; edit overlays are not');
  assert.equal(gltf.nodes.find((node) => node.name === 'Cube 01').translation[0], 2);
  assert.equal(gltf.animations, undefined);
  const primitive = gltf.meshes[0].primitives[0];
  assert.equal(gltf.accessors[primitive.attributes.POSITION].count, 16, 'the exported smooth mesh uses the edited vertices');
  const editPng = await downloadFrom(page, async () => { await page.locator('.export-menu summary').click(); await page.locator('.export-menu [data-export=png]').click(); });
  await page.keyboard.press('Escape');
  assert.equal(await page.locator('#mesh-mode-indicator').isVisible(), false);
  const objectPng = await downloadFrom(page, async () => { await page.locator('.export-menu summary').click(); await page.locator('.export-menu [data-export=png]').click(); });
  assert.deepEqual(editPng.bytes, objectPng.bytes, 'PNG excludes mesh overlays and transform handles');
  await page.locator('#file-menu summary').click(); await page.locator('#new-project').click();
  const choose = page.waitForEvent('filechooser');
  await page.locator('#file-menu summary').click(); await page.locator('#open-project').click();
  await (await choose).setFiles({ name: 'mesh.forma', mimeType: 'application/json', buffer: saved.bytes });
  await page.waitForFunction(() => document.querySelector('#status-text').textContent.includes('Project opened'));
  await page.getByRole('tab', { name: 'Model', exact: true }).click();
  await page.getByRole('button', { name: 'Edit mesh', exact: true }).click();
  assert.equal(await page.locator('#mesh-count').textContent(), '16 vertices · 14 faces');
  assert.equal(await page.getByRole('checkbox', { name: 'Smooth mesh shading' }).isChecked(), true);
  assert.deepEqual(errors, []);
  await context.close();
});

test('vertex selection, gizmo drag, snapping and undo operate on real geometry', async () => {
  const { page, context, errors } = await openPage();
  await page.locator('#file-menu summary').click(); await page.locator('#new-project').click();
  await page.getByRole('button', { name: 'Cube', exact: true }).click();
  await page.getByRole('button', { name: 'Frame selected object' }).click();
  await page.locator('#view-select').selectOption('front');
  await page.getByRole('tab', { name: 'Model', exact: true }).click();
  await page.getByRole('button', { name: 'Edit mesh', exact: true }).click();
  await page.getByRole('button', { name: 'Vertex', exact: true }).focus();
  await page.keyboard.press('Space');
  assert.equal(await page.getByRole('button', { name: 'Vertex', exact: true }).getAttribute('aria-pressed'), 'true');
  assert.equal(await page.getByRole('button', { name: 'Play animation' }).isVisible(), true, 'Space activates a focused button instead of starting playback');
  await page.getByRole('button', { name: 'Toggle transform snapping' }).click();
  assert.equal(await page.getByRole('button', { name: 'Toggle transform snapping' }).getAttribute('aria-pressed'), 'true');
  const canvas = await page.locator('#viewport-canvas canvas').boundingBox();
  // With a framed 1.4-unit cube, the front-facing upper-right vertex projects
  // to this location. The first click picks it; the next drag hits its red X axis.
  const extent = canvas.height / 2 * 0.7 / (Math.tan(20 * Math.PI / 180) * 1.4 * 2.6);
  const x = canvas.x + canvas.width / 2 + extent, y = canvas.y + canvas.height / 2 - extent;
  await page.mouse.click(x, y);
  assert.equal(await page.getByRole('spinbutton', { name: 'Vertex number' }).inputValue(), '7');
  assert.equal(await page.getByRole('spinbutton', { name: 'Vertex X' }).inputValue(), '0.7');
  await page.mouse.move(x + 38, y); await page.mouse.down();
  await page.mouse.move(x + 100, y, { steps: 12 }); await page.mouse.up();
  await page.waitForFunction(() => Number(document.querySelector('[data-vertex-axis="0"]').value) !== 0.7);
  const moved = Number(await page.getByRole('spinbutton', { name: 'Vertex X' }).inputValue());
  assert.ok(Math.abs(moved / 0.5 - Math.round(moved / 0.5)) < 1e-6, 'the local vertex position is snapped');
  await page.getByRole('button', { name: 'Undo', exact: true }).click();
  assert.equal(await page.getByRole('spinbutton', { name: 'Vertex X' }).inputValue(), '0.7');
  await page.getByRole('button', { name: 'Redo', exact: true }).click();
  assert.equal(Number(await page.getByRole('spinbutton', { name: 'Vertex X' }).inputValue()), moved);
  await page.getByRole('button', { name: 'Sphere', exact: true }).click();
  assert.equal(await page.locator('#mesh-mode-indicator').isVisible(), false, 'selecting a new object exits mesh mode');
  assert.equal(await page.getByRole('button', { name: 'Move tool' }).isEnabled(), true);
  assert.deepEqual(errors, []);
  await context.close();
});

test('a selected array copy places the vertex gizmo correctly under object scaling', async () => {
  const { page, context, errors } = await openPage();
  await page.locator('#file-menu summary').click(); await page.locator('#new-project').click();
  await page.getByRole('button', { name: 'Cube', exact: true }).click();
  await page.getByRole('checkbox', { name: 'Enable procedural array' }).check();
  await commitInput(page.locator('#array-count'), 3);
  await commitInput(page.getByRole('spinbutton', { name: 'Scale X' }), 1.5);
  await page.getByRole('button', { name: 'Frame selected object' }).click();
  await page.locator('#view-select').selectOption('front');
  await page.getByRole('tab', { name: 'Model', exact: true }).click();
  await page.getByRole('button', { name: 'Edit mesh', exact: true }).click();
  await page.getByRole('button', { name: 'Vertex', exact: true }).click();
  await page.getByRole('button', { name: 'Toggle transform snapping' }).click();
  const canvas = await page.locator('#viewport-canvas canvas').boundingBox();
  // Three copies at offsets -1.8, 0, 1.8, scaled 1.5 on X; framing uses
  // their 7.5-unit bounding width. Pick vertex 7 on the rightmost copy.
  const unit = canvas.height / 2 / (Math.tan(20 * Math.PI / 180) * 7.5 * 2.6);
  const x = canvas.x + canvas.width / 2 + (1.8 + 0.7) * 1.5 * unit;
  const y = canvas.y + canvas.height / 2 - 0.7 * unit;
  await page.mouse.click(x, y);
  assert.equal(await page.getByRole('spinbutton', { name: 'Vertex number' }).inputValue(), '7');
  await page.mouse.move(x + 55, y); await page.mouse.down();
  await page.mouse.move(x + 87, y, { steps: 8 }); await page.mouse.up();
  assert.equal(await page.getByRole('spinbutton', { name: 'Vertex X' }).inputValue(), '1');
  assert.equal(await page.getByRole('spinbutton', { name: 'Vertex Z' }).inputValue(), '0.7');
  const { bytes } = await downloadFrom(page, () => page.getByRole('button', { name: 'Save project', exact: true }).click());
  const object = JSON.parse(bytes.toString('utf8')).objects[0];
  assert.equal(object.array.count, 3);
  assert.equal(object.mesh.vertices[6][0], 1, 'every copy shares the edited vertex');
  assert.deepEqual(object.transform.scale, [1.5, 1, 1]);
  assert.deepEqual(errors, []);
  await context.close();
});

test('orthographic views keep equal-sized objects equal at different depths and support picking, zoom and framing', async () => {
  const { page, context, errors } = await openPage();
  const project = createStarterProject();
  const near = createObject('cube', 'near', 'Near cube'); near.transform.position = [-2.1, 1.5, 0]; near.material.color = '#ff0000';
  const far = createObject('cube', 'far', 'Far cube'); far.transform.position = [2.1, 1.5, -4]; far.material.color = '#00ff00';
  project.objects = [near, far]; project.camera = { position: [0, 1.5, 10], target: [0, 1.5, 0] };
  const chooser = page.waitForEvent('filechooser');
  await page.locator('#file-menu summary').click(); await page.locator('#open-project').click();
  await (await chooser).setFiles({ name: 'views.forma', mimeType: 'application/json', buffer: Buffer.from(JSON.stringify(project)) });
  await page.waitForFunction(() => document.querySelector('#status-text').textContent.includes('Project opened'));
  await page.locator('#view-select').selectOption('front');
  assert.equal(await page.locator('#projection-toggle').getAttribute('aria-pressed'), 'true');
  await page.locator('#toggle-grid').click();
  const box = await page.locator('#viewport-canvas canvas').boundingBox();
  await page.mouse.click(box.x + box.width * 0.1, box.y + box.height * 0.9); // deselect overlays
  let pixels = await colorRegions(page);
  assert.ok(pixels.red && pixels.green, 'both cubes are visible');
  const width = (region) => region.maxX - region.minX + 1;
  assert.ok(Math.abs(width(pixels.red) - width(pixels.green)) <= 2, 'orthographic projection ignores depth when sizing identical cubes');
  await page.mouse.click(box.x + (pixels.green.minX + pixels.green.maxX) / 2 / pixels.width * box.width,
    box.y + (pixels.green.minY + pixels.green.maxY) / 2 / pixels.height * box.height);
  assert.equal(await page.getByRole('button', { name: 'Select Far cube' }).getAttribute('aria-pressed'), 'true', 'raycasting uses the orthographic camera');
  await page.keyboard.press('q'); // remove the gizmo without changing the selected object
  await page.locator('#projection-toggle').click();
  assert.equal(await page.locator('#projection-toggle').getAttribute('aria-pressed'), 'false');
  pixels = await colorRegions(page);
  assert.ok(width(pixels.red) > width(pixels.green) * 1.3, 'perspective projection makes the distant cube smaller');
  await page.locator('#projection-toggle').click();
  pixels = await colorRegions(page);
  const beforeZoom = width(pixels.green);
  await page.mouse.move(box.x + box.width * 0.6, box.y + box.height * 0.6);
  await page.mouse.wheel(0, -240);
  pixels = await colorRegions(page);
  const afterZoom = width(pixels.green);
  assert.ok(afterZoom > beforeZoom, 'mouse wheel changes orthographic zoom');
  const saved = await downloadFrom(page, () => page.getByRole('button', { name: 'Save project', exact: true }).click());
  const savedCamera = JSON.parse(saved.bytes.toString('utf8')).camera;
  const savedDistance = Math.hypot(...savedCamera.position.map((value, index) => value - savedCamera.target[index]));
  assert.ok(Math.abs(savedDistance - 10 * beforeZoom / afterZoom) < 0.3, 'v2 camera pose approximates the zoomed orthographic target-plane framing');
  await page.setViewportSize({ width: 375, height: 812 });
  await page.getByRole('button', { name: 'Frame selected object' }).click();
  pixels = await colorRegions(page);
  assert.ok(pixels.green, 'framed object is visible at a narrow aspect ratio');
  assert.ok(pixels.green.minX > 5 && pixels.green.maxX < pixels.width - 6, 'framing fits the entire object horizontally');
  assert.ok(pixels.green.minY > 5 && pixels.green.maxY < pixels.height - 6, 'framing fits the entire object vertically');
  assert.ok(Math.abs(width(pixels.green) - (pixels.green.maxY - pixels.green.minY + 1)) <= 3, 'resizing does not stretch the square');
  assert.deepEqual(errors, []);
  await context.close();
});

test('six axis presets, opposite numpad shortcuts and camera settings remain compatible with v2 projects', async () => {
  const { page, context, errors } = await openPage();
  await page.evaluate(() => {
    window.cameraSaves = [];
    window.forma = { saveProject: async (text) => { window.cameraSaves.push(JSON.parse(text)); return { canceled: false }; } };
  });
  const directions = { front: [0, 0, 1], back: [0, 0, -1], right: [1, 0, 0], left: [-1, 0, 0], top: [0, 1, 0], bottom: [0, -1, 0] };
  let saveCount = 0;
  for (const [name, expected] of Object.entries(directions)) {
    await page.locator('#view-select').selectOption(name);
    assert.equal(await page.locator('#projection-toggle').getAttribute('aria-pressed'), 'true', `${name} should use orthographic projection`);
    await page.locator('#save-project').click();
    await page.waitForFunction((count) => window.cameraSaves.length === count && !document.querySelector('#save-project').disabled, ++saveCount);
    const saved = await page.evaluate(() => window.cameraSaves.at(-1));
    assert.equal(saved.version, 2);
    assert.deepEqual(Object.keys(saved.camera).sort(), ['position', 'target'], 'projection remains a session-only setting in the existing format');
    const delta = saved.camera.position.map((value, index) => value - saved.camera.target[index]);
    const length = Math.hypot(...delta);
    delta.forEach((value, index) => assert.ok(Math.abs(value / length - expected[index]) < 1e-5, `${name} axis ${index}`));
  }
  await page.evaluate(() => document.activeElement.blur());
  await page.keyboard.press('Numpad1');
  assert.equal(await page.locator('#view-select').inputValue(), 'front');
  await page.keyboard.press('Control+Numpad1');
  assert.equal(await page.locator('#view-select').inputValue(), 'back');
  await page.keyboard.press('Numpad3');
  assert.equal(await page.locator('#view-select').inputValue(), 'right');
  await page.keyboard.press('Control+Numpad3');
  assert.equal(await page.locator('#view-select').inputValue(), 'left');
  await page.keyboard.press('Numpad7');
  assert.equal(await page.locator('#view-select').inputValue(), 'top');
  await page.keyboard.press('Control+Numpad7');
  assert.equal(await page.locator('#view-select').inputValue(), 'bottom');
  await page.keyboard.press('Numpad5');
  assert.equal(await page.locator('#projection-toggle').getAttribute('aria-pressed'), 'false');
  await page.locator('#view-select').selectOption('front');
  const canvas = await page.locator('#viewport-canvas canvas').boundingBox();
  await page.mouse.move(canvas.x + canvas.width * 0.82, canvas.y + canvas.height * 0.65);
  await page.mouse.down(); await page.mouse.move(canvas.x + canvas.width * 0.72, canvas.y + canvas.height * 0.55, { steps: 8 }); await page.mouse.up();
  await page.waitForFunction(() => document.querySelector('#view-select').value === 'custom');
  assert.deepEqual(errors, []);
  await context.close();
});

test('proportional numeric edits, falloff, history, v2 save/open and GLB/PNG exports', async () => {
  const { page, context, errors } = await openPage();
  await page.locator('#file-menu summary').click(); await page.locator('#new-project').click();
  await page.getByRole('button', { name: 'Plane', exact: true }).click();
  await page.getByRole('tab', { name: 'Model', exact: true }).click();
  await page.getByRole('button', { name: 'Edit mesh', exact: true }).click();
  await page.getByRole('button', { name: 'Subdivide mesh' }).click();
  await page.getByRole('button', { name: 'Subdivide mesh' }).click();
  assert.equal(await page.locator('#mesh-count').textContent(), '25 vertices · 16 faces');
  await page.getByRole('button', { name: 'Vertex', exact: true }).click();
  await commitInput(page.getByRole('spinbutton', { name: 'Vertex number' }), 5); // center of the original plane
  await page.getByRole('checkbox', { name: 'Proportional vertex editing' }).check();
  await commitInput(page.getByRole('spinbutton', { name: /Influence radius/ }), 2);
  await page.getByLabel('Falloff').selectOption('linear');
  await commitInput(page.getByRole('spinbutton', { name: 'Vertex Y' }), 1);
  assert.equal(await page.getByRole('spinbutton', { name: 'Vertex Y' }).inputValue(), '1');
  await page.getByRole('button', { name: 'Undo', exact: true }).click();
  assert.equal(await page.getByRole('spinbutton', { name: 'Vertex Y' }).inputValue(), '0');
  await page.getByRole('button', { name: 'Redo', exact: true }).click();
  assert.equal(await page.getByRole('spinbutton', { name: 'Vertex Y' }).inputValue(), '1');
  await commitInput(page.getByRole('spinbutton', { name: /Influence radius/ }), 0);
  assert.equal(await page.getByRole('spinbutton', { name: /Influence radius/ }).inputValue(), '2');
  assert.match(await page.locator('#status-text').textContent(), /radius/);
  await page.getByRole('checkbox', { name: 'Smooth mesh shading' }).check();
  const saved = await downloadFrom(page, () => page.getByRole('button', { name: 'Save project', exact: true }).click());
  const scene = JSON.parse(saved.bytes.toString('utf8'));
  const mesh = scene.objects[0].mesh;
  assert.equal(scene.version, 2); assert.equal(mesh.vertices.length, 25);
  assert.equal(mesh.vertices[4][1], 1);
  assert.equal(mesh.vertices[5][1], 0.5, 'a vertex one unit away gets half the movement with a two-unit linear radius');
  assert.ok(Math.abs(mesh.vertices[0][1] - (1 - Math.SQRT2 / 2)) < 1e-10);
  assert.equal('proportional' in scene, false, 'tool settings are not written into the project format');
  const glb = await downloadFrom(page, async () => { await page.locator('.export-menu summary').click(); await page.locator('.export-menu [data-export=glb]').click(); });
  const jsonLength = glb.bytes.readUInt32LE(12);
  const gltf = JSON.parse(glb.bytes.subarray(20, 20 + jsonLength).toString('utf8').trim());
  const accessor = gltf.accessors[gltf.meshes[0].primitives[0].attributes.POSITION];
  const view = gltf.bufferViews[accessor.bufferView];
  const binaryStart = 20 + jsonLength + 8;
  const vertexY = (index) => glb.bytes.readFloatLE(binaryStart + (view.byteOffset || 0) + (accessor.byteOffset || 0) + index * (view.byteStride || 12) + 4);
  assert.equal(accessor.count, 25);
  assert.ok(Math.abs(vertexY(4) - 1) < 1e-6 && Math.abs(vertexY(5) - 0.5) < 1e-6, 'GLB contains the deformed vertex positions');
  const editPng = await downloadFrom(page, async () => { await page.locator('.export-menu summary').click(); await page.locator('.export-menu [data-export=png]').click(); });
  await page.keyboard.press('Escape');
  const objectPng = await downloadFrom(page, async () => { await page.locator('.export-menu summary').click(); await page.locator('.export-menu [data-export=png]').click(); });
  assert.deepEqual(editPng.bytes, objectPng.bytes, 'influence rings and points are excluded from PNG');
  await page.locator('#file-menu summary').click(); await page.locator('#new-project').click();
  const choose = page.waitForEvent('filechooser');
  await page.locator('#file-menu summary').click(); await page.locator('#open-project').click();
  await (await choose).setFiles({ name: 'terrain.forma', mimeType: 'application/json', buffer: saved.bytes });
  await page.waitForFunction(() => document.querySelector('#status-text').textContent.includes('Project opened'));
  await page.getByRole('tab', { name: 'Model', exact: true }).click();
  await page.getByRole('button', { name: 'Edit mesh', exact: true }).click();
  await page.getByRole('button', { name: 'Vertex', exact: true }).click();
  await commitInput(page.getByRole('spinbutton', { name: 'Vertex number' }), 5);
  assert.equal(await page.getByRole('spinbutton', { name: 'Vertex Y' }).inputValue(), '1');
  await page.evaluate(() => document.activeElement.blur());
  await page.keyboard.press('o');
  assert.equal(await page.getByRole('checkbox', { name: 'Proportional vertex editing' }).isChecked(), false);
  await commitInput(page.getByRole('spinbutton', { name: 'Vertex Y' }), 1.2);
  const after = await downloadFrom(page, () => page.getByRole('button', { name: 'Save project', exact: true }).click());
  const afterMesh = JSON.parse(after.bytes.toString('utf8')).objects[0].mesh;
  assert.equal(afterMesh.vertices[4][1], 1.2);
  assert.equal(afterMesh.vertices[5][1], 0.5, 'disabled proportional editing moves only the selected vertex');
  assert.deepEqual(errors, []);
  await context.close();
});

test('proportional gizmo drag uses a fixed baseline and undoes as one edit', async () => {
  const { page, context, errors } = await openPage();
  await page.locator('#file-menu summary').click(); await page.locator('#new-project').click();
  await page.getByRole('button', { name: 'Plane', exact: true }).click();
  await page.getByRole('button', { name: 'Frame selected object' }).click();
  await page.locator('#view-select').selectOption('front');
  await page.getByRole('tab', { name: 'Model', exact: true }).click();
  await page.getByRole('button', { name: 'Edit mesh', exact: true }).click();
  await page.getByRole('button', { name: 'Subdivide mesh' }).click();
  await page.getByRole('button', { name: 'Vertex', exact: true }).click();
  await commitInput(page.getByRole('spinbutton', { name: 'Vertex number' }), 5);
  await page.getByRole('checkbox', { name: 'Proportional vertex editing' }).check();
  await commitInput(page.getByRole('spinbutton', { name: /Influence radius/ }), 2);
  await page.getByLabel('Falloff').selectOption('linear');
  const canvas = await page.locator('#viewport-canvas canvas').boundingBox();
  const x = canvas.x + canvas.width / 2, y = canvas.y + canvas.height / 2;
  await page.mouse.move(x, y - 42); await page.mouse.down();
  await page.mouse.move(x, y - 115, { steps: 15 }); await page.mouse.up();
  await page.waitForFunction(() => Math.abs(Number(document.querySelector('[data-vertex-axis="1"]').value)) > 0.05);
  const saved = await downloadFrom(page, () => page.getByRole('button', { name: 'Save project', exact: true }).click());
  const mesh = JSON.parse(saved.bytes.toString('utf8')).objects[0].mesh;
  const delta = mesh.vertices[4][1];
  assert.ok(delta > 0.05);
  assert.ok(Math.abs(mesh.vertices[5][1] - delta * 0.5) < 1e-7, 'neighbor weight remains based on the start of the drag');
  assert.ok(Math.abs(mesh.vertices[0][1] - delta * (1 - Math.SQRT2 / 2)) < 1e-7);
  await page.getByRole('button', { name: 'Undo', exact: true }).click();
  assert.equal(await page.getByRole('spinbutton', { name: 'Vertex Y' }).inputValue(), '0');
  await page.getByRole('button', { name: 'Redo', exact: true }).click();
  assert.ok(Math.abs(Number(await page.getByRole('spinbutton', { name: 'Vertex Y' }).inputValue()) - delta) < 1e-6);
  assert.deepEqual(errors, []);
  await context.close();
});

test('editor panels and help dialog have no automated accessibility violations', async () => {
  const { page, context } = await openPage();
  for (const state of ['Object', 'Model', 'Model face edit', 'Model vertex edit', 'Model proportional edit', 'Material', 'Scene', 'Help']) {
    if (state === 'Help') await page.getByRole('button', { name: 'Help and shortcuts' }).click();
    else if (state === 'Model face edit') await page.getByRole('button', { name: 'Edit mesh', exact: true }).click();
    else if (state === 'Model vertex edit') await page.getByRole('button', { name: 'Vertex', exact: true }).click();
    else if (state === 'Model proportional edit') await page.getByRole('checkbox', { name: 'Proportional vertex editing' }).check();
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
    await page.getByRole('tab', { name: 'Model', exact: true }).click();
    await page.getByRole('button', { name: 'Edit mesh', exact: true }).click();
    await page.getByRole('button', { name: 'Vertex', exact: true }).click();
    await commitInput(page.getByRole('spinbutton', { name: 'Vertex number' }), 7);
    assert.equal(await page.getByRole('spinbutton', { name: 'Vertex X' }).inputValue(), '0.7');
    await page.getByRole('checkbox', { name: 'Proportional vertex editing' }).check();
    await commitInput(page.getByRole('spinbutton', { name: /Influence radius/ }), 2);
    assert.equal(await page.getByLabel('Falloff').inputValue(), 'smooth');
    assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1), true, `model tools overflow at ${width}`);
    const top = await page.locator('.viewport-top').boundingBox();
    for (const selector of ['#view-select', '#projection-toggle', '#toggle-grid', '#toggle-snap', '#wireframe']) {
      const bounds = await page.locator(selector).boundingBox();
      assert.ok(bounds.x >= top.x && bounds.x + bounds.width <= top.x + top.width + 1, `${selector} is visible at ${width}`);
    }
    assert.deepEqual(errors, []);
    await context.close();
  }
});
