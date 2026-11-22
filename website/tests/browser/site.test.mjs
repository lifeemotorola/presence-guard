import test, { before, after } from 'node:test';
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { dirname, extname, resolve, sep } from 'node:path';
import { fileURLToPath } from 'node:url';
import { chromium } from 'playwright-core';
import AxeBuilder from '@axe-core/playwright';
import { PRODUCTS } from '../../catalog.js';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const mime = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.svg': 'image/svg+xml', '.webp': 'image/webp', '.woff2': 'font/woff2' };
let server, browser, base;

before(async () => {
  server = createServer(async (req, res) => {
    const pathname = new URL(req.url, 'http://test.local').pathname;
    const path = resolve(root, `.${pathname === '/' ? '/index.html' : pathname}`);
    if (!path.startsWith(root + sep)) { res.writeHead(403).end(); return; }
    try { const bytes = await readFile(path); res.writeHead(200, { 'Content-Type': mime[extname(path)] || 'application/octet-stream' }); res.end(bytes); }
    catch { res.writeHead(404).end(); }
  });
  await new Promise((resolveListen) => server.listen(0, '0.0.0.0', resolveListen));
  base = `http://127.0.0.1:${server.address().port}`;
  browser = await chromium.launch({ executablePath: process.env.CHROMIUM_PATH || '/usr/bin/google-chrome', headless: true, args: ['--no-sandbox', '--disable-dev-shm-usage'] });
});
after(async () => { await browser?.close(); await new Promise((resolveClose) => server?.close(resolveClose)); });

async function openPage({ assets = PRODUCTS.map((product) => product.asset), status = 200, width = 1440, height = 900, date = '2026-11-20T22:00:00Z' } = {}) {
  const context = await browser.newContext({ viewport: { width, height } });
  const page = await context.newPage();
  const errors = [];
  page.on('pageerror', (error) => errors.push(error.message));
  await page.route('https://api.github.com/**', (route) => route.fulfill({
    status, contentType: 'application/json', headers: { 'access-control-allow-origin': '*' },
    body: JSON.stringify({ draft: false, assets: assets.map((name) => ({ name, state: 'uploaded' })) }),
  }));
  await page.clock.setFixedTime(new Date(date));
  await page.goto(base, { waitUntil: 'domcontentloaded' });
  await page.waitForFunction(() => !document.querySelector('#download-status').textContent.includes('Checking available downloads'));
  return { page, errors };
}

test('catalog, search, categories and empty state work with public ZIPs', async () => {
  const { page, errors } = await openPage();
  assert.equal(await page.locator('.tool-card').count(), 14);
  assert.match(await page.locator('#download-status').innerText(), /Direct ZIPs/);
  assert.equal(await page.getByRole('link', { name: 'Download DockBloom ZIP' }).getAttribute('href'), 'https://github.com/lifeemotorola/presence-guard/releases/download/creator-downloads/dockbloom-windows-x64.zip');
  await page.getByRole('button', { name: /Windows apps/ }).click();
  assert.equal(await page.locator('.tool-card').count(), 3);
  assert.equal(await page.getByRole('button', { name: /Windows apps/ }).getAttribute('aria-pressed'), 'true');
  await page.getByRole('button', { name: /All tools/ }).click();
  await page.getByRole('searchbox', { name: 'Search tools' }).fill('audio mixer');
  assert.equal(await page.locator('.tool-card').count(), 1);
  assert.equal(await page.locator('.tool-card h3').innerText(), 'MixBloom');
  await page.getByRole('searchbox', { name: 'Search tools' }).fill('no such tool');
  assert.equal(await page.locator('.tool-card').count(), 0);
  assert.equal(await page.locator('#empty-state').isVisible(), true);
  await page.getByRole('button', { name: /Show all tools/ }).click();
  assert.equal(await page.locator('.tool-card').count(), 14);
  assert.deepEqual(errors, []);
  await page.close();
});

test('details, accurate requirements, download links, and Escape focus return', async () => {
  const { page, errors } = await openPage();
  const details = page.getByRole('button', { name: 'View DockBloom details' });
  await details.click();
  assert.equal(await page.locator('dialog').isVisible(), true);
  assert.match(await page.locator('dialog').innerText(), /OBS Studio 33\.0\.1/);
  assert.match(await page.locator('dialog').innerText(), /Install\.bat/);
  assert.equal(await page.locator('dialog').getByRole('link', { name: 'Download DockBloom ZIP' }).getAttribute('href'), 'https://github.com/lifeemotorola/presence-guard/releases/download/creator-downloads/dockbloom-windows-x64.zip');
  await page.keyboard.press('Escape');
  assert.equal(await page.locator('dialog').isVisible(), false);
  assert.equal(await details.evaluate((element) => document.activeElement === element), true);
  await page.getByRole('button', { name: 'View Forma Studio details' }).click();
  assert.match(await page.locator('dialog').innerText(), /basic face and vertex editing/i);
  assert.match(await page.locator('dialog').innerText(), /no AI, sculpting or rigging/i);
  assert.match(await page.locator('dialog').innerText(), /0\.2 and 0\.3 projects cannot be opened in 0\.1/i);
  assert.match(await page.locator('dialog').innerText(), /proportional movement, orthographic views/i);
  assert.match(await page.locator('dialog').innerText(), /There is no autosave or cloud backup/i);
  assert.equal(await page.locator('dialog').getByRole('link', { name: 'Download Forma Studio ZIP' }).getAttribute('href'), 'https://github.com/lifeemotorola/presence-guard/releases/download/creator-downloads/forma-studio-windows-x64.zip');
  await page.getByRole('button', { name: 'Close details' }).click();
  await page.getByRole('button', { name: 'View ScreenForge details' }).click();
  assert.match(await page.locator('dialog').innerText(), /ffmpeg\.exe/);
  await page.getByRole('button', { name: 'Close details' }).click();
  await page.getByRole('button', { name: 'View Face Retouch details' }).click();
  assert.match(await page.locator('dialog').innerText(), /UXP Developer Tool/);
  assert.match(await page.locator('dialog').innerText(), /not a \.ccx installer/i);
  await page.getByRole('button', { name: 'Close details' }).click();
  await page.getByRole('button', { name: 'View Presence Guard details' }).click();
  assert.match(await page.locator('dialog').innerText(), /OBS 33 has not been verified/);
  assert.deepEqual(errors, []);
  await page.close();
});

test('unavailable and partial releases show honest Actions fallbacks', async () => {
  const unavailable = await openPage({ status: 404 });
  assert.match(await unavailable.page.locator('#download-status').innerText(), /sign-in may be required and artifacts can expire/);
  assert.equal(await unavailable.page.getByRole('link', { name: 'Open the DockBloom build on GitHub' }).getAttribute('href'), 'https://github.com/lifeemotorola/presence-guard/actions/runs/44787973114');
  await unavailable.page.getByRole('button', { name: 'View DockBloom details' }).click();
  assert.match(await unavailable.page.locator('dialog').innerText(), /obs-dockbloom-windows-x64/);
  await unavailable.page.close();
  const partial = await openPage({ assets: ['dockbloom-windows-x64.zip'], date: '2030-01-01T00:00:00Z' });
  assert.match(await partial.page.locator('#download-status').innerText(), /1 of 14/);
  assert.equal(await partial.page.getByRole('link', { name: 'Open the MixBloom build on GitHub' }).getAttribute('href'), 'https://github.com/lifeemotorola/presence-guard/actions/workflows/mixbloom-windows.yml');
  assert.equal(await partial.page.getByRole('link', { name: 'Open the Forma Studio build on GitHub' }).getAttribute('href'), 'https://github.com/lifeemotorola/presence-guard/actions/workflows/forma-studio.yml');
  await partial.page.close();
});

test('the catalog and details meet automated accessibility checks', async () => {
  const { page } = await openPage({ status: 404 });
  const pageResults = await new AxeBuilder({ page }).withTags(['wcag2a', 'wcag2aa', 'wcag21a', 'wcag21aa']).analyze();
  assert.deepEqual(pageResults.violations.map((violation) => violation.id), []);
  await page.getByRole('button', { name: 'View DockBloom details' }).click();
  const dialogResults = await new AxeBuilder({ page }).withTags(['wcag2a', 'wcag2aa', 'wcag21a', 'wcag21aa']).analyze();
  assert.deepEqual(dialogResults.violations.map((violation) => violation.id), []);
  await page.close();
});

test('mobile and tablet layouts do not overflow and dialogs remain usable', async () => {
  for (const [width, height] of [[320, 640], [375, 812], [768, 1024], [1440, 900]]) {
    const { page, errors } = await openPage({ width, height });
    assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), true, `horizontal overflow at ${width}px`);
    assert.equal(await page.locator('.hero-art img').evaluate((img) => img.complete && img.naturalWidth > 0), true);
    await page.getByRole('button', { name: 'View DockBloom details' }).click();
    const box = await page.locator('dialog').boundingBox();
    assert.ok(box && box.x >= 0 && box.x + box.width <= width && box.height <= height, `dialog outside ${width}px viewport`);
    assert.equal(await page.locator('dialog').getByRole('link', { name: 'Download DockBloom ZIP' }).count(), 1);
    await page.keyboard.press('Escape');
    assert.deepEqual(errors, []);
    await page.close();
  }
});
