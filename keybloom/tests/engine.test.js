/* KeyBloom tests. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const E = require('../app/engine.js');
const C = require('../app/content.js');

function lesson(text = 'abcd') { return new E.TypingRun({ texts: [text], kind: 'lesson' }); }
function timed(durationMs = 60000) { return new E.TypingRun({ texts: ['ab', 'cd'], kind: 'test', durationMs }); }

test('the original course covers the full alphabet and has unique, usable lessons', () => {
  assert.equal(C.lessons.length, 21);
  assert.equal(new Set(C.lessons.map((x) => x.id)).size, C.lessons.length);
  assert.ok(C.lessons.every((x) => /^[a-z0-9-]+$/.test(x.id) && x.title && x.group && x.tip && x.text.length > 20));
  assert.ok(C.lessons.every((x) => /^[\x20-\x7e]+$/.test(x.text)), 'the course uses printable US keyboard characters');
  const combined = C.lessons.map((x) => x.text.toLowerCase()).join('');
  for (const character of 'abcdefghijklmnopqrstuvwxyz') assert.ok(combined.includes(character), character);
  assert.ok(C.passages.length >= 6 && C.sprintPassages.length >= 3);
});

test('course introduces letters before asking for them in drills', () => {
  const additions = ['fj', 'dk', 'sl', 'a;', 'gh', '', 'ru', 'ty', 'ei', 'wo', 'qp', '', 'vm', 'bn', 'c,', 'x.', 'z/', '', '0123456789', "'?!", ''];
  const available = new Set([' ']);
  C.lessons.forEach((item, index) => {
    for (const char of additions[index].toLowerCase()) available.add(char);
    for (const char of item.text.toLowerCase()) assert.ok(available.has(char), `${item.id} uses ${JSON.stringify(char)} too early`);
  });
});

test('rejects missing texts and invalid timed runs', () => {
  assert.throws(() => new E.TypingRun({ texts: [] }));
  assert.throws(() => new E.TypingRun({ texts: ['ok'], kind: 'test' }));
  assert.throws(() => new E.TypingRun({ texts: ['ok'], kind: 'other' }));
});

test('timer starts on the first inserted character, not when the lesson opens', () => {
  const run = lesson();
  assert.equal(run.snapshot(9000).elapsedMs, 0);
  run.input('', 9000);
  run.input('a', 10000);
  assert.equal(run.snapshot(12000).elapsedMs, 2000);
});

test('finished lesson computes correct-position WPM and attempted-key accuracy', () => {
  const run = lesson();
  run.input('a', 0);
  const result = run.input('abcd', 12000);
  assert.equal(result.finished, true);
  assert.equal(result.reason, 'complete');
  assert.equal(result.attempts, 4);
  assert.equal(result.correct, 4);
  assert.equal(result.accuracy, 100);
  assert.equal(result.wpm, 4);
  assert.equal(result.words, 1);
});

test('mistakes still count after a backspace correction', () => {
  const run = lesson('abc');
  run.input('a', 0);
  run.input('ax', 1000);
  run.input('a', 1500);
  run.input('ab', 2000);
  const result = run.input('abc', 3000);
  assert.equal(result.attempts, 4);
  assert.equal(result.mistakes, 1);
  assert.equal(result.accuracy, 75);
  assert.equal(result.uncorrected, 0);
  assert.equal(result.correct, 3);
});

test('finishes even with uncorrected mistakes and reports them', () => {
  const run = lesson('abc');
  run.input('a', 0);
  const result = run.input('axc', 3000);
  assert.equal(result.finished, true);
  assert.equal(result.correct, 2);
  assert.equal(result.uncorrected, 1);
  assert.equal(result.accuracy, 67);
});

test('replacement, selection and deletion count inserted characters rather than backspaces', () => {
  const run = lesson('abcd');
  run.input('abx', 0);
  run.input('abc', 1000);
  assert.equal(run.snapshot(1000).attempts, 4);
  assert.equal(run.snapshot(1000).mistakes, 1);
  run.input('ab', 1500);
  assert.equal(run.snapshot(1500).attempts, 4);
});

test('pause freezes time and input until resumed', () => {
  const run = lesson();
  run.input('a', 100);
  run.pause(1100);
  assert.equal(run.snapshot(9000).elapsedMs, 1000);
  assert.equal(run.input('ab', 9000).typed, 'a');
  run.resume(10100);
  assert.equal(run.snapshot(11100).elapsedMs, 2000);
  assert.equal(run.input('ab', 11100).attempts, 2);
});

test('pausing before a run starts does not start its clock', () => {
  const run = lesson();
  assert.equal(run.pause(500).paused, false);
  assert.equal(run.snapshot(10000).elapsedMs, 0);
});

test('timed tests continue to another passage without losing the score', () => {
  const run = timed();
  run.input('a', 0);
  let snapshot = run.input('ab', 2000);
  assert.equal(snapshot.target, 'cd');
  assert.equal(snapshot.typed, '');
  assert.equal(snapshot.correct, 2);
  snapshot = run.input('c', 3000);
  assert.equal(snapshot.correct, 3);
  assert.equal(snapshot.attempts, 3);
});

test('timed test stops at its exact limit and ignores later input', () => {
  const run = timed(5000);
  run.input('a', 0);
  const result = run.tick(7000);
  assert.equal(result.finished, true);
  assert.equal(result.reason, 'time');
  assert.equal(result.elapsedMs, 5000);
  assert.equal(result.remainingMs, 0);
  assert.equal(run.input('ab', 8000).typed, 'a');
});

test('a paused timed test does not expire until its active time is used', () => {
  const run = timed(5000);
  run.input('a', 0);
  run.pause(2000);
  assert.equal(run.tick(20000).finished, false);
  run.resume(20000);
  assert.equal(run.tick(22999).finished, false);
  assert.equal(run.tick(23000).finished, true);
});

test('word count only rewards correctly typed complete words', () => {
  assert.equal(E.correctWords('one two three', 'one twx three'), 2);
  assert.equal(E.correctWords('one two three', 'on'), 0);
  assert.equal(E.correctWords("don't stop", "don't st"), 1);
});

test('progress keeps summaries only and does not save the exercise or typed text', () => {
  const run = lesson('secret');
  run.input('s', 0);
  const result = run.input('secret', 10000);
  const state = E.addRecord(E.defaultProfile(), result, new Date(2026, 0, 2), 'home-fj');
  assert.equal(state.records.length, 1);
  assert.equal(state.records[0].lessonId, 'home-fj');
  assert.ok(!JSON.stringify(state).includes('secret'));
  assert.equal(state.records[0].day, '2026-01-02');
});

test('unfinished runs and zero-attempt timed runs are not recorded', () => {
  const run = lesson();
  assert.equal(E.addRecord(E.defaultProfile(), run.snapshot(1000)).records.length, 0);
  assert.equal(E.addRecord(E.defaultProfile(), { kind: 'test', finished: true, attempts: 0 }).records.length, 0);
});

test('corrupt or hostile stored data is sanitised and record count is bounded', () => {
  const valid = { kind: 'test', day: '2026-01-02', wpm: 42, accuracy: 90, durationMs: 60000, attempts: 20, mistakes: 2, text: '<script>' };
  const state = E.sanitizeProfile({ settings: { goalMinutes: 999, keyboard: 'yes' }, records: [
    ...Array(520).fill(valid), { ...valid, day: '2026-02-30' }, { ...valid, mistakes: 21 }, { ...valid, wpm: NaN },
  ] });
  assert.equal(state.settings.goalMinutes, 10);
  assert.equal(state.settings.keyboard, true);
  assert.equal(state.records.length, 497);
  assert.ok(!JSON.stringify(state).includes('<script>'));
  assert.equal(E.sanitizeProfile(null).records.length, 0);
});

test('local date and streak handle year boundaries, leap days, and missed days', () => {
  assert.equal(E.localDay(new Date(2026, 0, 2, 23)), '2026-01-02');
  const rows = [{ day: '2025-12-31' }, { day: '2026-01-01' }, { day: '2026-01-01' }, { day: '2026-01-02' }];
  assert.equal(E.streak(rows, new Date(2026, 0, 2, 10)), 3);
  assert.equal(E.streak(rows, new Date(2026, 0, 3, 10)), 3);
  assert.equal(E.streak(rows, new Date(2026, 0, 4, 10)), 0);
  assert.notEqual(E.dayNumber('2024-02-29'), null);
  assert.equal(E.dayNumber('2025-02-29'), null);
});

test('stats use weighted accuracy and unique completed lessons', () => {
  const record = { kind: 'lesson', day: '2026-01-02', lessonId: 'home-fj', wpm: 30, accuracy: 90, durationMs: 30000, attempts: 10, mistakes: 1 };
  const state = E.sanitizeProfile({ records: [record, { ...record, attempts: 90, mistakes: 9, wpm: 40 }] });
  const stats = E.stats(state, new Date(2026, 0, 2));
  assert.equal(stats.accuracy, 90);
  assert.equal(stats.bestWpm, 40);
  assert.equal(stats.lessonsCompleted, 1);
  assert.equal(stats.todayMs, 60000);
  assert.equal(stats.streak, 1);
  assert.equal(E.recommendedLesson(C.lessons, state).id, 'home-dk');
});
