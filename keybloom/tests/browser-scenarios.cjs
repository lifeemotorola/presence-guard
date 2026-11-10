/* KeyBloom end-to-end checks. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const STORAGE_KEY = 'keybloom.progress.v1';
const nav = (page, label) => page.getByRole('button', { name: label, exact: true });
const heading = (page, text) => page.getByRole('heading', { name: text, exact: true });
async function recordData(page) { return page.evaluate((key) => JSON.parse(localStorage.getItem(key)), STORAGE_KEY); }
async function noHorizontalOverflow(page, name) {
  const dims = await page.evaluate(() => {
    const space = document.querySelector('#workspace');
    return { body: [document.body.scrollWidth, document.body.clientWidth], main: [space.scrollWidth, space.clientWidth] };
  });
  assert.ok(dims.body[0] <= dims.body[1] + 2, `${name}: body overflows horizontally: ${JSON.stringify(dims)}`);
  assert.ok(dims.main[0] <= dims.main[1] + 2, `${name}: main area overflows horizontally: ${JSON.stringify(dims)}`);
}
async function expectVisible(locator, label) {
  await locator.waitFor({ state: 'visible', timeout: 15000 });
  assert.ok(await locator.isVisible(), label || 'expected an element to be visible');
}

async function runScenarios(page, options = {}) {
  const errors = [];
  page.on('pageerror', (error) => errors.push(error.stack || error.message));
  await expectVisible(heading(page, 'Good to see you.'));
  assert.equal(await page.title(), 'KeyBloom — Your typing studio');
  assert.equal(await page.locator('.stat-card').count(), 3);
  if (options.screenshot) {
    fs.mkdirSync(path.dirname(options.screenshot), { recursive: true });
    await page.screenshot({ path: options.screenshot });
  }
  await noHorizontalOverflow(page, 'dashboard');

  await nav(page, 'Lessons').click();
  assert.equal(await page.locator('.lesson-card').count(), 21);
  await noHorizontalOverflow(page, 'lessons');
  await page.getByRole('button', { name: 'Start: Find your anchors' }).click();
  await expectVisible(heading(page, 'Find your anchors'));
  assert.equal(await page.locator('.key.next').innerText(), 'F');
  assert.equal(await page.locator('#pause-button').isDisabled(), true);
  const firstText = (await page.locator('#target-accessible').textContent()).replace('Practice text: ', '');
  const input = page.locator('#typing-input');
  await input.pressSequentially('x');
  assert.equal(await page.locator('.target-char.wrong').count(), 1);
  const pasted = await input.evaluate((node) => {
    const transfer = new DataTransfer();
    transfer.setData('text/plain', 'not typed');
    const event = new ClipboardEvent('paste', { bubbles: true, cancelable: true, clipboardData: transfer });
    node.dispatchEvent(event);
    return event.defaultPrevented;
  });
  assert.equal(pasted, true, 'paste must be blocked during practice');
  assert.match(await page.locator('#run-message').textContent(), /pasting is off/i);
  await input.press('Backspace');
  await input.pressSequentially(firstText, { delay: 30 });
  await expectVisible(heading(page, 'A little better, already.'));
  assert.match(await page.locator('.result-detail').textContent(), /1 mistake/);
  let saved = await recordData(page);
  assert.equal(saved.records.length, 1);
  assert.equal(saved.records[0].lessonId, 'home-fj');
  assert.equal(saved.records[0].accuracy, Math.round(firstText.length / (firstText.length + 1) * 100));
  assert.ok(!JSON.stringify(saved).includes(firstText), 'typed content must not be saved');

  await nav(page, 'Overview').click();
  await expectVisible(page.getByRole('button', { name: /Continue learning/ }));
  assert.match(await page.locator('.path-row').first().textContent(), /Meet D and K/);
  await page.reload();
  await expectVisible(page.getByRole('button', { name: /Continue learning/ }));
  assert.equal((await recordData(page)).records.length, 1, 'progress survives a reload');

  await nav(page, 'Lessons').click();
  await page.getByRole('button', { name: 'Start: Make it a capital' }).click();
  assert.equal(await page.locator('.key.next').innerText(), 'A');
  assert.equal(await page.locator('.key.shift-needed').count(), 2);
  await page.locator('#typing-input').pressSequentially('A');
  await page.getByRole('button', { name: 'Pause', exact: true }).click();
  assert.equal(await page.locator('#typing-input').getAttribute('readonly'), '');
  await expectVisible(page.getByRole('button', { name: 'Resume', exact: true }));
  await page.getByRole('button', { name: 'Resume', exact: true }).click();
  assert.equal(await page.locator('#typing-input').getAttribute('readonly'), null);
  await nav(page, 'Overview').click();
  await expectVisible(heading(page, 'Leave this practice?'));
  await page.getByRole('button', { name: 'Cancel', exact: true }).click();
  assert.equal(await page.locator('#typing-input').getAttribute('readonly'), null);
  await nav(page, 'Overview').click();
  await page.getByRole('button', { name: 'Leave practice' }).click();
  assert.equal((await recordData(page)).records.length, 1, 'abandoned practice does not save');

  await nav(page, 'Settings').click();
  await noHorizontalOverflow(page, 'settings');
  await page.locator('#goal-select').selectOption('15');
  await page.locator('#keyboard-setting').uncheck();
  await page.reload();
  await nav(page, 'Settings').click();
  assert.equal(await page.locator('#goal-select').inputValue(), '15');
  assert.equal(await page.locator('#keyboard-setting').isChecked(), false);
  await page.getByRole('button', { name: 'Reset progress', exact: true }).click();
  await expectVisible(heading(page, 'Reset all progress?'));
  await page.getByRole('button', { name: 'Cancel', exact: true }).click();
  assert.equal((await recordData(page)).records.length, 1);
  await page.getByRole('button', { name: 'Reset progress', exact: true }).click();
  await page.getByRole('dialog').getByRole('button', { name: 'Reset progress' }).click();
  saved = await recordData(page);
  assert.equal(saved.records.length, 0);
  assert.equal(saved.settings.goalMinutes, 10);
  assert.equal(saved.settings.keyboard, true);

  // Fake time permits testing the complete timed experience without making CI
  // wait 1 minute and 45 seconds. The engine also has real-clock unit tests.
  await page.clock.install({ time: new Date('2026-10-31T12:00:00Z') });
  await nav(page, 'Practice').click();
  await noHorizontalOverflow(page, 'practice');
  await page.getByRole('button', { name: /Start a typing test/ }).click();
  await expectVisible(heading(page, 'Typing test'));
  await page.locator('#typing-input').pressSequentially('Every');
  await page.clock.fastForward(60000);
  await expectVisible(heading(page, 'A little better, already.'));
  saved = await recordData(page);
  assert.equal(saved.records.length, 1);
  assert.equal(saved.records[0].kind, 'test');
  assert.equal(saved.records[0].durationMs, 60000);

  await nav(page, 'Practice').click();
  await page.getByRole('button', { name: /Play word sprint/ }).click();
  await expectVisible(heading(page, 'Word sprint'));
  await page.locator('#typing-input').pressSequentially('bloom ');
  assert.equal((await page.locator('#run-words').textContent()).trim(), '1');
  await page.evaluate(() => window.dispatchEvent(new Event('blur')));
  assert.equal(await page.locator('#typing-input').getAttribute('readonly'), '', 'window blur pauses practice');
  await page.clock.fastForward(60000);
  await expectVisible(heading(page, 'Word sprint'));
  await page.getByRole('button', { name: 'Resume', exact: true }).click();
  await page.clock.fastForward(45000);
  await expectVisible(heading(page, 'A little better, already.'));
  assert.match(await page.locator('.result-metrics').textContent(), /1correct words/);
  saved = await recordData(page);
  assert.equal(saved.records[1].kind, 'sprint');
  assert.equal(saved.records[1].words, 1);

  await nav(page, 'My progress').click();
  assert.equal(await page.locator('.history-row').count(), 2);
  await noHorizontalOverflow(page, 'progress');
  assert.deepEqual(errors, [], 'no uncaught renderer errors');
  console.log('Browser scenarios: passed (lessons, errors, paste, pause, persistence, settings, timed test, sprint, progress).');
}
async function runLayoutScenarios(page) {
  await page.setViewportSize({ width: 900, height: 640 });
  const check = async (name) => {
    await noHorizontalOverflow(page, name);
    const bad = await page.evaluate(() => [...document.querySelectorAll('#workspace button, #workspace input, #workspace select')]
      .filter((node) => { const r = node.getBoundingClientRect(); return r.width && (r.left < 0 || r.right > innerWidth + 1); })
      .map((node) => node.textContent.trim().slice(0, 50)));
    assert.deepEqual(bad, [], `${name}: controls extend beyond the window`);
  };
  for (const name of ['Overview', 'Lessons', 'Practice', 'My progress', 'Settings']) {
    await nav(page, name).click();
    await check(`minimum-size ${name}`);
  }
  await nav(page, 'Lessons').click();
  await page.getByRole('button', { name: 'Start: Find your anchors' }).click();
  await check('minimum-size lesson');
  const keyboard = await page.locator('.keyboard').evaluate((node) => [node.scrollWidth, node.clientWidth]);
  assert.ok(keyboard[0] <= keyboard[1] + 2, `keyboard is cut off: ${keyboard}`);
  await page.locator('#typing-input').pressSequentially('f');
  await nav(page, 'Overview').click();
  await expectVisible(heading(page, 'Leave this practice?'));
  const dialog = await page.getByRole('dialog').boundingBox();
  assert.ok(dialog.x >= 0 && dialog.y >= 0 && dialog.x + dialog.width <= 900 && dialog.y + dialog.height <= 640, 'confirmation dialog fits the window');
  await page.getByRole('button', { name: 'Leave practice' }).click();
  console.log('Minimum-sized layouts and controls: passed (900 × 640).');
}
module.exports = { runScenarios, runLayoutScenarios, noHorizontalOverflow };
