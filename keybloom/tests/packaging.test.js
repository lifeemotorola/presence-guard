/* KeyBloom package checks. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const ROOT = path.resolve(__dirname, '..');
const read = (file) => fs.readFileSync(path.join(ROOT, file), 'utf8');

test('the app has a local entry, stylesheet, scripts, icons and illustration', () => {
  const html = read('app/index.html');
  for (const match of html.matchAll(/(?:src|href)="([^"]+)"/g)) {
    if (match[1].startsWith('#')) continue;
    assert.ok(!/^[a-z]+:/i.test(match[1]), `remote resource: ${match[1]}`);
    assert.ok(fs.existsSync(path.join(ROOT, 'app', match[1])), `missing: ${match[1]}`);
  }
  for (const file of ['app/assets/studio-illustration.jpg', 'app/assets/icon.ico', 'app/assets/icon.png', 'desktop/main.js', 'INSTALL.txt']) {
    assert.ok(fs.existsSync(path.join(ROOT, file)), `missing: ${file}`);
  }
});

test('renderer policy denies connections and desktop host limits privileged access', () => {
  const html = read('app/index.html');
  const main = read('desktop/main.js');
  assert.match(html, /connect-src 'none'/);
  assert.match(html, /object-src 'none'/);
  assert.match(main, /nodeIntegration:\s*false/);
  assert.match(main, /contextIsolation:\s*true/);
  assert.match(main, /sandbox:\s*true/);
  assert.match(main, /setPermissionRequestHandler/);
  assert.match(main, /setWindowOpenHandler/);
  assert.match(main, /onBeforeRequest/);
});

test('Windows package has an explicit publisher and pinned dependencies', () => {
  const pkg = JSON.parse(read('package.json'));
  const lock = JSON.parse(read('package-lock.json'));
  assert.equal(pkg.author, 'Emmanuel Suah');
  assert.equal(pkg.license, 'GPL-2.0-or-later');
  assert.equal(pkg.main, 'desktop/main.js');
  for (const [name, version] of Object.entries(pkg.devDependencies)) {
    assert.match(version, /^\d+\.\d+\.\d+$/);
    assert.equal(lock.packages[`node_modules/${name}`].version, version);
  }
  assert.match(read('scripts/package-windows.cjs'), /CompanyName:\s*'Emmanuel Suah'/);
});
