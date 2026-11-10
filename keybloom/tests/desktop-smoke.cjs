/* KeyBloom Windows smoke test. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { _electron: electron } = require('playwright');
const { runScenarios, runLayoutScenarios } = require('./browser-scenarios.cjs');

(async () => {
  const exe = path.resolve(process.env.KEYBLOOM_EXE || path.join(__dirname, '..', 'out', 'KeyBloom-win32-x64', 'KeyBloom.exe'));
  if (!fs.existsSync(exe)) throw new Error(`Packaged application is missing: ${exe}`);
  const dataDir = fs.mkdtempSync(path.join(os.tmpdir(), 'keybloom-test-'));
  const env = { ...process.env, KEYBLOOM_TEST_DATA_DIR: dataDir };
  let desktop;
  try {
    desktop = await electron.launch({ executablePath: exe, env, timeout: 60000 });
    const page = await desktop.firstWindow({ timeout: 60000 });
    await page.waitForLoadState('domcontentloaded');
    assert.deepEqual(await page.evaluate(() => ({ require: typeof window.require, process: typeof window.process })),
      { require: 'undefined', process: 'undefined' }, 'renderer must not expose Node');
    assert.equal(await page.evaluate(async () => {
      try { await fetch('https://example.com/'); return false; } catch { return true; }
    }), true, 'renderer network must be blocked');
    await runScenarios(page, { screenshot: process.env.KEYBLOOM_SCREENSHOT });
    await runLayoutScenarios(page);
    await desktop.close();
    desktop = await electron.launch({ executablePath: exe, env, timeout: 60000 });
    const reopened = await desktop.firstWindow({ timeout: 60000 });
    await reopened.getByRole('heading', { name: 'Good to see you.' }).waitFor();
    const data = await reopened.evaluate(() => JSON.parse(localStorage.getItem('keybloom.progress.v1')));
    assert.equal(data.records.length, 2, 'saved progress must survive quitting and restarting the packaged app');
    assert.equal(data.records[0].kind, 'test');
    assert.equal(data.records[1].kind, 'sprint');
    console.log('Packaged Windows app: launch, security, navigation, typing, and restart passed.');
  } finally {
    if (desktop) await desktop.close().catch(() => {});
    fs.rmSync(dataDir, { recursive: true, force: true });
  }
})().catch((error) => { console.error(error); process.exitCode = 1; });
