/* KeyBloom story and file-practice checks. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const { createHash } = require('node:crypto');
const { readFileSync } = require('node:fs');
const { join } = require('node:path');
const E = require('../app/engine.js');
const S = require('../app/stories.js');
const storyHashes = require('./story-hashes.json');

test('the offline library contains 80 distinct, complete long stories from varied sources', () => {
  assert.equal(S.all.length, 80);
  assert.equal(new Set(S.all.map((story) => story.id)).size, 80);
  assert.equal(new Set(S.all.map((story) => story.title.toLowerCase())).size, 80);
  assert.equal(new Set(S.all.map((story) => story.topic.toLowerCase())).size, 80);
  assert.equal(new Set(S.all.map((story) => story.text.toLowerCase())).size, 80);
  assert.equal(new Set(S.all.map((story) => story.sourceId)).size, 7);
  assert.equal(Object.keys(storyHashes).length, 80);
  const credits = readFileSync(join(__dirname, '..', 'STORY-CREDITS.txt'), 'utf8').toLowerCase();
  for (const story of S.all) {
    assert.match(story.id, /^[a-z0-9-]+$/);
    assert.ok(story.source && story.source.author && story.source.book, story.id);
    assert.equal(story.wordCount, story.text.trim().split(/\s+/).length);
    assert.ok(story.wordCount >= 700, `${story.title} has only ${story.wordCount} words`);
    assert.ok((story.text.match(/\b[A-Za-z]+(?:['-][A-Za-z]+)*/g) || []).length >= 700, `too few alphabetic words: ${story.title}`);
    assert.match(story.text, /^[\x20-\x7e]+$/, story.title);
    assert.doesNotMatch(story.text, /\*\*\* END|\[Illustration|II\. MISCELLANEOUS TALES/i);
    assert.ok(/[.!?]["']*$/.test(story.text), `story should have an ending: ${story.title}`);
    // These digests were checked against the source entries, after the documented
    // typography and note removal. Catch later truncation or unintended changes.
    assert.equal(createHash('sha256').update(story.text).digest('hex'), storyHashes[story.id], story.title);
    assert.ok(credits.includes(story.title.toLowerCase()), `missing story credit: ${story.title}`);
  }
  // Boundary checks guard against accidentally bundling an excerpt rather than
  // each original story's beginning and ending.
  const magi = S.all.find((story) => story.title === 'The Gift of the Magi');
  assert.ok(magi.text.startsWith('One dollar and eighty-seven cents.'));
  assert.ok(magi.text.endsWith('Everywhere they are wisest. They are the magi.'));
  const rice = S.all.find((story) => story.title === 'My Lord Bag of Rice');
  assert.match(rice.text, /^Long, long ago/);
  assert.match(rice.text, /henceforth known as My Lord Bag of Rice\.$/);
  const lilies = S.all.find((story) => story.title === 'The Water-Lily. The Gold-Spinners');
  assert.match(lilies.text, /^Once upon a time, in a large forest/);
  assert.match(lilies.text, /her two sisters lived happily ever after\.$/);
  const lab = S.all.find((story) => story.title === 'A Slip Under the Microscope');
  assert.match(lab.text, /^Outside the laboratory windows/);
  assert.match(lab.text, /destined to make unanswered remarks\.$/);
});

test('every story can be selected and the immediately previous one is excluded uniformly', () => {
  for (let index = 0; index < S.all.length; index += 1) {
    assert.equal(S.choose('', () => (index + 0.25) / S.all.length).id, S.all[index].id);
  }
  for (const previous of [S.all[0], S.all[39], S.all[79]]) {
    const picks = Array.from({ length: 79 }, (_, index) => S.choose(previous.id, () => (index + 0.25) / 79).id);
    assert.equal(new Set(picks).size, 79);
    assert.ok(!picks.includes(previous.id));
    assert.deepEqual(new Set(picks), new Set(S.all.filter((story) => story.id !== previous.id).map((story) => story.id)));
  }
  assert.equal(S.choose('unknown', () => 0).id, S.all[0].id);
  assert.throws(() => S.choose('', () => 1), RangeError);
  assert.throws(() => S.choose('', () => NaN), RangeError);
});

test('each story supports an untimed run with accurate results and no text stored', () => {
  for (const story of S.all) {
    const run = new E.TypingRun({ kind: 'story', texts: [story.text] });
    run.input(story.text.slice(0, 1), 0);
    const result = run.input(story.text, 60000);
    assert.equal(result.finished, true, story.title);
    assert.equal(result.reason, 'complete');
    assert.equal(result.remainingMs, null);
    assert.equal(result.correct, story.text.length);
    assert.equal(result.accuracy, 100);
    const stored = E.addRecord(E.defaultProfile(), result, new Date(2026, 0, 2), story.id);
    assert.equal(stored.records[0].kind, 'story');
    assert.equal(stored.records[0].lessonId, '');
    assert.ok(!JSON.stringify(stored).includes(story.text.slice(0, 80)));
  }
});

test('file text normalizes common typography and validates without truncating', () => {
  assert.deepEqual(E.prepareImportedText('\uFEFF  Café\n“bright”—day…  '), {
    text: 'Cafe "bright"--day...', changed: true, error: '',
  });
  assert.deepEqual(E.prepareImportedText('A simple ASCII file.'), { text: 'A simple ASCII file.', changed: false, error: '' });
  assert.match(E.prepareImportedText('short').error, /at least 10/);
  assert.match(E.prepareImportedText('The sun is ☀ today.').error, /unsupported characters/);
  assert.match(E.prepareImportedText('Hello\0 there world.').error, /unsupported characters/);
  assert.match(E.prepareImportedText('a'.repeat(E.IMPORT_MAX_CHARS + 1)).error, /80,000/);
  assert.equal(E.prepareImportedText('a'.repeat(E.IMPORT_MAX_CHARS)).text.length, E.IMPORT_MAX_CHARS);
  assert.equal(E.IMPORT_MAX_BYTES, 256 * 1024);
  assert.equal(E.prepareCustomText('x'.repeat(501)).error.includes('500'), true, 'the original editor keeps its 500-character limit');
});

test('import runs and hostile imported records never retain filename, passage, or letter counts', () => {
  const secret = 'Private file contents. Hello there.';
  const filename = 'secret-filename.txt';
  const run = new E.TypingRun({ kind: 'import', texts: [secret] });
  run.input('X', 0);
  run.input('', 50);
  const result = run.input(secret, 5000);
  assert.equal(result.reason, 'complete');
  assert.deepEqual(result.keyStats, {});
  const profile = E.addRecord(E.defaultProfile(), { ...result, keyStats: { p: { attempts: 1, mistakes: 1 } }, filename, text: secret }, new Date(2026, 0, 2));
  assert.deepEqual(profile.records[0].keyStats, {});
  assert.equal(profile.records[0].kind, 'import');
  assert.ok(!JSON.stringify(profile).includes(secret));
  assert.ok(!JSON.stringify(profile).includes(filename));
  const hostile = E.sanitizeProfile({ records: [{ ...profile.records[0], filename, text: secret, keyStats: { p: { attempts: 1, mistakes: 1 } } }] });
  assert.deepEqual(hostile.records[0].keyStats, {});
  assert.ok(!JSON.stringify(hostile).includes(secret));
  assert.ok(!JSON.stringify(hostile).includes(filename));
});

test('finishing long practice early saves only the active time and remains optional', () => {
  for (const kind of ['story', 'import']) {
    const run = new E.TypingRun({ kind, texts: ['A long practice text.'] });
    assert.equal(run.finishEarly(100).finished, false, 'an unstarted run cannot be finished');
    run.input('A', 1000);
    run.pause(3000);
    const result = run.finishEarly(10000);
    assert.equal(result.finished, true);
    assert.equal(result.reason, 'stopped');
    assert.equal(result.elapsedMs, 2000);
    assert.equal(result.attempts, 1);
    assert.equal(E.addRecord(E.defaultProfile(), result).records[0].kind, kind);
  }
  const ordinary = new E.TypingRun({ kind: 'lesson', texts: ['hello'] });
  ordinary.input('h', 0);
  assert.equal(ordinary.finishEarly(1000).finished, false, 'early finish is restricted to the two long modes');
});

test('cached scoring remains correct after edits, pauses, and repeated timer snapshots', () => {
  const text = 'one two three ' + 'long '.repeat(16000);
  const run = new E.TypingRun({ kind: 'import', texts: [text] });
  run.input('one txo th', 0);
  let result = run.snapshot(1000);
  assert.equal(result.correct, E.correctPositions(text, 'one txo th'));
  assert.equal(result.words, E.correctWords(text, 'one txo th'));
  assert.equal(run.snapshot(2000).correct, result.correct);
  run.input('one two th', 3000);
  result = run.snapshot(4000);
  assert.equal(result.correct, E.correctPositions(text, 'one two th'));
  assert.equal(result.words, 2);
  run.input('one two three ' + 'long '.repeat(10000), 5000);
  result = run.snapshot(6000);
  assert.equal(result.correct, result.typed.length);
  assert.equal(result.words, 10003);
});
