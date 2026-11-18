/* KeyBloom Windows smoke test. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { _electron: electron } = require('playwright');
const { runScenarios, runAdvancedScenarios, runLayoutScenarios } = require('./browser-scenarios.cjs');

(async () => {
  const exe = path.resolve(process.env.KEYBLOOM_EXE || path.join(__dirname, '..', 'out', 'KeyBloom-win32-x64', 'KeyBloom.exe'));
  if (!fs.existsSync(exe)) throw new Error(`Packaged application is missing: ${exe}`);
  const dataDir = fs.mkdtempSync(path.join(os.tmpdir(), 'keybloom-test-'));
  const env = { ...process.env, KEYBLOOM_TEST_DATA_DIR: dataDir };
  const movedDir = fs.mkdtempSync(path.join(os.tmpdir(), 'keybloom-moved-'));
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
    await runAdvancedScenarios(page, { screenshot: process.env.KEYBLOOM_FEATURE_SCREENSHOT });
    await runLayoutScenarios(page);
    await desktop.close();
    desktop = null;
    // Upgrades are often extracted to a new folder; verify that changing the
    // executable path does not strand the user's existing profile.
    fs.cpSync(path.dirname(exe), movedDir, { recursive: true });
    desktop = await electron.launch({ executablePath: path.join(movedDir, path.basename(exe)), env, timeout: 60000 });
    const reopened = await desktop.firstWindow({ timeout: 60000 });
    await reopened.getByRole('heading', { name: 'Good to see you.' }).waitFor();
    const data = await reopened.evaluate(() => JSON.parse(localStorage.getItem('keybloom.progress.v1')));
    assert.equal(data.records.length, 7, 'saved progress must survive quitting and restarting the packaged app');
    assert.deepEqual(data.records.map((row) => row.kind), ['test', 'sprint', 'lesson', 'focus', 'custom', 'test', 'test']);
    assert.deepEqual(data.records[4].keyStats, {}, 'custom passage letter counts must not be saved');
    assert.equal(data.settings.practiceSize, 'extra-large');
    assert.ok(!JSON.stringify(data).includes('A <script> grows.'), 'custom passage must not persist across restarts');
    console.log('Packaged Windows app: security, new features, moved-folder restart, and progress passed.');
  } finally {
    if (desktop) await desktop.close().catch(() => {});
    fs.rmSync(dataDir, { recursive: true, force: true, maxRetries: 5, retryDelay: 500 });
    fs.rmSync(movedDir, { recursive: true, force: true, maxRetries: 5, retryDelay: 500 });
  }
})().catch((error) => { console.error(error); process.exitCode = 1; });
