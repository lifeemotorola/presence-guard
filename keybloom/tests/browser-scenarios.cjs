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
async function runAdvancedScenarios(page, options = {}) {
  // Simulate a 1.0 profile: past records exist, but per-key counts and the
  // text-size preference do not. Old progress must survive the upgrade.
  const old = await recordData(page);
  delete old.settings.practiceSize;
  for (const record of old.records) delete record.keyStats;
  await page.evaluate(({ key, value }) => localStorage.setItem(key, JSON.stringify(value)), { key: STORAGE_KEY, value: old });
  await page.reload();
  await nav(page, 'My progress').click();
  assert.equal(await page.locator('.history-row').count(), 2, '1.0 history survives upgrade');
  assert.match(await page.locator('.insight-empty').textContent(), /Older sessions/);
  assert.equal(await page.locator('.milestone').count(), 6);
  assert.match(await page.locator('.milestone').filter({ hasText: 'First bloom' }).textContent(), /Reached/);

  // A corrected error is attributed to the expected F, and can launch a drill.
  await nav(page, 'Lessons').click();
  await page.getByRole('button', { name: 'Start: Find your anchors' }).click();
  const lessonTarget = (await page.locator('#target-accessible').textContent()).replace('Practice text: ', '');
  await page.locator('#typing-input').pressSequentially('x');
  await page.locator('#typing-input').press('Backspace');
  await page.locator('#typing-input').pressSequentially(lessonTarget);
  await expectVisible(heading(page, 'A little better, already.'));
  let saved = await recordData(page);
  assert.equal(saved.records.length, 3);
  assert.equal(saved.records[2].keyStats.f.mistakes, 1);
  assert.equal(saved.records[2].keyStats.x, undefined, 'typed wrong letters are not stored');
  await nav(page, 'My progress').click();
  const focusButton = page.getByRole('button', { name: /Practice F, 1 mistake/ });
  await expectVisible(focusButton);
  await focusButton.click();
  await expectVisible(heading(page, 'Focus on F'));
  assert.equal(await page.locator('.key.next').innerText(), 'F');
  const focusTarget = (await page.locator('#target-accessible').textContent()).replace('Practice text: ', '');
  assert.ok(focusTarget.split(' ').every((word) => word.includes('f')));
  await page.locator('#typing-input').pressSequentially(focusTarget);
  await expectVisible(heading(page, 'A little better, already.'));
  saved = await recordData(page);
  assert.equal(saved.records[3].kind, 'focus');
  await nav(page, 'My progress').click();
  assert.match(await page.locator('.milestone').filter({ hasText: 'Practice with purpose' }).textContent(), /Reached/);
  if (options.screenshot) {
    fs.mkdirSync(path.dirname(options.screenshot), { recursive: true });
    await page.locator('.milestones-section').scrollIntoViewIfNeeded();
    await page.screenshot({ path: options.screenshot });
  }

  // Personal passages can be pasted in the editor, never the scoring box,
  // and neither the passage nor its per-letter statistics are saved.
  await nav(page, 'Practice').click();
  await page.getByRole('button', { name: /Add your text/ }).click();
  await expectVisible(heading(page, 'Practice with your own text.'));
  const editor = page.locator('#custom-text');
  assert.equal(await page.locator('#start-custom').isDisabled(), true);
  await editor.fill('too short');
  assert.match(await page.locator('#custom-error').textContent(), /at least 10/);
  await editor.fill('A smile and emoji ☀');
  assert.match(await page.locator('#custom-error').textContent(), /printable US keyboard/);
  const allowed = await editor.evaluate((node) => {
    const event = new ClipboardEvent('paste', { bubbles: true, cancelable: true });
    node.dispatchEvent(event);
    return !event.defaultPrevented;
  });
  assert.equal(allowed, true, 'paste should be allowed in the editor');
  const draft = 'A <script> grows.\nLovely words.';
  const normalized = 'A <script> grows. Lovely words.';
  await editor.fill(draft);
  assert.equal(await page.locator('#start-custom').isEnabled(), true);
  await nav(page, 'Overview').click();
  await expectVisible(heading(page, 'Discard your text?'));
  await page.getByRole('button', { name: 'Cancel', exact: true }).click();
  assert.equal(await editor.inputValue(), draft, 'cancel keeps the unsaved draft');
  await page.locator('#start-custom').click();
  await expectVisible(heading(page, 'Your custom practice'));
  assert.equal(await page.locator('#target-text').textContent(), normalized);
  assert.equal(await page.locator('#target-text script').count(), 0, 'passage markup is not executed as HTML');
  const blocked = await page.locator('#typing-input').evaluate((node) => {
    const event = new ClipboardEvent('paste', { bubbles: true, cancelable: true });
    node.dispatchEvent(event);
    return event.defaultPrevented;
  });
  assert.equal(blocked, true, 'scored custom input still prevents paste');
  await page.locator('#typing-input').pressSequentially(normalized);
  await expectVisible(heading(page, 'A little better, already.'));
  saved = await recordData(page);
  assert.equal(saved.records[4].kind, 'custom');
  assert.deepEqual(saved.records[4].keyStats, {});
  assert.ok(!JSON.stringify(saved).includes(normalized), 'personal passage is never stored');
  await page.getByRole('button', { name: /Try again/ }).click();
  assert.equal(await page.locator('#target-text').textContent(), normalized, 'retry uses only the in-memory passage');
  await page.locator('#typing-input').pressSequentially('A');
  await nav(page, 'Practice').click();
  await page.getByRole('button', { name: 'Leave practice' }).click();
  assert.equal((await recordData(page)).records.length, 5, 'abandoned retry does not save');
  await nav(page, 'My progress').click();
  assert.match(await page.locator('.history-list').textContent(), /Custom practice/);

  // Text size is saved and affects actual rendered practice text.
  await nav(page, 'Settings').click();
  await page.locator('#practice-size').selectOption('extra-large');
  await page.reload();
  await nav(page, 'Settings').click();
  assert.equal(await page.locator('#practice-size').inputValue(), 'extra-large');
  await nav(page, 'Lessons').click();
  await page.getByRole('button', { name: 'Start: Meet D and K' }).click();
  assert.equal(await page.locator('#target-text').evaluate((node) => getComputedStyle(node).fontSize), '30px');
  assert.equal(await page.locator('#typing-input').evaluate((node) => getComputedStyle(node).fontSize), '23px');
  await nav(page, 'Practice').click();

  // Both new test lengths complete at exactly their limits under a mock clock.
  for (const seconds of [30, 300]) {
    await page.locator('#test-duration').selectOption(String(seconds));
    await page.getByRole('button', { name: /Start a typing test/ }).click();
    await expectVisible(heading(page, 'Typing test'));
    await page.locator('#typing-input').pressSequentially('Every');
    await page.clock.fastForward(seconds * 1000);
    await expectVisible(heading(page, 'A little better, already.'));
    saved = await recordData(page);
    assert.equal(saved.records.at(-1).durationMs, seconds * 1000);
    assert.equal(saved.records.at(-1).kind, 'test');
    await nav(page, 'Practice').click();
  }
  assert.equal((await recordData(page)).records.length, 7);
  console.log('Advanced scenarios: passed (1.0 migration, key insights, focused drill, milestones, private custom text, size, new test lengths).');
}

async function runLayoutScenarios(page) {
  await page.setViewportSize({ width: 900, height: 640 });
  const check = async (name) => {
    await noHorizontalOverflow(page, name);
    const bad = await page.evaluate(() => [...document.querySelectorAll('#workspace button, #workspace input, #workspace select, #workspace textarea')]
      .filter((node) => { const r = node.getBoundingClientRect(); return r.width && (r.left < 0 || r.right > innerWidth + 1); })
      .map((node) => node.textContent.trim().slice(0, 50)));
    assert.deepEqual(bad, [], `${name}: controls extend beyond the window`);
    const clipped = await page.evaluate(() => [...document.querySelectorAll('.challenge-card button, .challenge-card select, .custom-card textarea, .custom-card button, .focus-insight')]
      .filter((node) => {
        const parent = node.closest('.challenge-card, .custom-card, .insights-card');
        if (!parent) return false;
        const r = node.getBoundingClientRect(), p = parent.getBoundingClientRect();
        return r.width && (r.left < p.left - 1 || r.right > p.right + 1 || r.top < p.top - 1 || r.bottom > p.bottom + 1);
      }).map((node) => node.textContent.trim().slice(0, 50)));
    assert.deepEqual(clipped, [], `${name}: controls are clipped by a card`);
  };
  for (const name of ['Overview', 'Lessons', 'Practice', 'My progress', 'Settings']) {
    await nav(page, name).click();
    await check(`minimum-size ${name}`);
  }
  await page.locator('#practice-size').selectOption('extra-large');
  await nav(page, 'Practice').click();
  await page.getByRole('button', { name: /Add your text/ }).click();
  await check('minimum-size custom editor');
  await page.locator('#custom-text').fill('Practice with my own text.');
  await nav(page, 'Overview').click();
  await expectVisible(heading(page, 'Discard your text?'));
  await page.getByRole('button', { name: 'Discard text' }).click();
  await nav(page, 'Practice').click();
  await page.locator('#focus-key').selectOption('z');
  await page.getByRole('button', { name: /Practice this key/ }).click();
  await expectVisible(heading(page, 'Focus on Z'));
  await check('minimum-size focus drill');
  await nav(page, 'Lessons').click();
  await page.getByRole('button', { name: /^(Start:|Practice again:) Find your anchors$/ }).click();
  await check('minimum-size large-text lesson');
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
module.exports = { runScenarios, runAdvancedScenarios, runLayoutScenarios, noHorizontalOverflow };
