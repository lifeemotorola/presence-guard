/*
 * KeyBloom — host-independent typing and progress logic.
 * Copyright (C) 2026 Emmanuel Suah
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.KeyBloomEngine = factory();
})(typeof globalThis === 'undefined' ? this : globalThis, function () {
  'use strict';

  const STORAGE_KEY = 'keybloom.progress.v1';
  const MAX_RECORDS = 500;
  // Course completion is separate from the rolling session history.
  const MAX_COMPLETED_LESSONS = 2000;
  const GOALS = [5, 10, 15, 20];
  const PRACTICE_SIZES = ['standard', 'large', 'extra-large'];
  const CUSTOM_MIN = 10;
  const CUSTOM_MAX = 500;
  const IMPORT_MAX_BYTES = 256 * 1024;
  const IMPORT_MAX_CHARS = 80000;
  const LETTERS = 'abcdefghijklmnopqrstuvwxyz';
  const KINDS = new Set(['lesson', 'test', 'sprint', 'focus', 'custom', 'import', 'story']);
  const UNTIMED = new Set(['lesson', 'focus', 'custom', 'import', 'story']);
  const PRIVATE_KINDS = new Set(['custom', 'import']);
  const clamp = (value, min, max) => Math.max(min, Math.min(max, value));
  const finite = (value, fallback = 0) => Number.isFinite(Number(value)) ? Number(value) : fallback;

  // Custom exercises stay in memory only. Limiting these to printable US
  // keyboard characters also keeps character positions and hints consistent.
  function prepareCustomText(value) {
    const text = typeof value === 'string' ? value.replace(/[\t\r\n]+/g, ' ').replace(/ +/g, ' ').trim() : '';
    if (text.length < CUSTOM_MIN) return { text, error: `Please enter at least ${CUSTOM_MIN} characters.` };
    if (text.length > CUSTOM_MAX) return { text, error: `Please use no more than ${CUSTOM_MAX} characters.` };
    if (!/^[\x20-\x7e]+$/.test(text)) return { text, error: 'Use printable US keyboard characters only; emojis and accented characters are not supported yet.' };
    return { text, error: '' };
  }

  // File content is never saved. Convert common word-processor punctuation,
  // accents, and whitespace to typable US-QWERTY characters, but reject other
  // characters instead of silently dropping or truncating the user's words.
  function prepareImportedText(value) {
    const original = typeof value === 'string' ? value.replace(/^\uFEFF/, '') : '';
    const text = original
      .replace(/[\u2018\u2019\u201A\u201B]/g, "'")
      .replace(/[\u201C\u201D\u201E\u201F]/g, '"')
      .replace(/\u2014/g, '--').replace(/[\u2010-\u2013\u2015]/g, '-')
      .replace(/\u2026/g, '...')
      .normalize('NFKD').replace(/[\u0300-\u036f]/g, '')
      .replace(/\s+/g, ' ').trim();
    const changed = text !== original.trim();
    if (text.length < CUSTOM_MIN) return { text, changed, error: `Please choose a file with at least ${CUSTOM_MIN} characters of text.` };
    if (text.length > IMPORT_MAX_CHARS) return { text: '', changed, error: `Please choose a file with no more than ${IMPORT_MAX_CHARS.toLocaleString('en-US')} characters after formatting.` };
    if (!/^[\x20-\x7e]+$/.test(text)) return { text: '', changed, error: 'This file contains unsupported characters. Use English text and printable US keyboard characters; emoji and other scripts are not supported.' };
    return { text, changed, error: '' };
  }

  function correctPositions(target, typed) {
    let count = 0;
    for (let i = 0; i < typed.length && i < target.length; i += 1) {
      if (typed[i] === target[i]) count += 1;
    }
    return count;
  }

  function correctWords(target, typed) {
    let count = 0;
    for (const match of target.matchAll(/[a-z0-9]+(?:'[a-z]+)?/gi)) {
      const end = match.index + match[0].length;
      if (end > typed.length) break;
      if (typed.slice(match.index, end) === match[0]) count += 1;
    }
    return count;
  }

  /* Return the inserted section, including replacements, not deleted keys. */
  function insertedRange(before, after) {
    let start = 0;
    while (start < before.length && start < after.length && before[start] === after[start]) start += 1;
    let suffix = 0;
    while (suffix < before.length - start && suffix < after.length - start &&
           before[before.length - 1 - suffix] === after[after.length - 1 - suffix]) suffix += 1;
    return { start, text: after.slice(start, after.length - suffix) };
  }

  class TypingRun {
    constructor({ texts, kind = 'lesson', durationMs = null }) {
      if (!Array.isArray(texts) || !texts.length || texts.some((text) => typeof text !== 'string' || !text.length)) {
        throw new Error('A typing run needs non-empty practice text.');
      }
      if (!KINDS.has(kind)) throw new Error('Unknown practice kind.');
      if (!UNTIMED.has(kind) && (!Number.isFinite(durationMs) || durationMs <= 0)) {
        throw new Error('A timed run needs a positive duration.');
      }
      this.texts = texts.slice();
      this.kind = kind;
      this.durationMs = UNTIMED.has(kind) ? null : durationMs;
      this.index = 0;
      this.typed = '';
      this.attempts = 0;
      this.mistakes = 0;
      this.keyStats = {};
      this.finishedCorrect = 0;
      this.finishedWords = 0;
      this.finishedIncorrect = 0;
      this.started = false;
      this.paused = false;
      this.finished = false;
      this.reason = null;
      this.activeMs = 0;
      this.resumedAt = null;
      this.finalElapsed = null;
      this.scoredTarget = null;
      this.scoredTyped = null;
      this.scoredCorrect = 0;
      this.scoredWords = 0;
    }

    get target() { return this.texts[this.index]; }

    elapsed(now) {
      if (!this.started) return 0;
      if (this.finalElapsed !== null) return this.finalElapsed;
      return this.activeMs + (this.paused ? 0 : Math.max(0, finite(now) - this.resumedAt));
    }

    tick(now) {
      if (!this.finished && this.started && this.durationMs !== null && this.elapsed(now) >= this.durationMs) {
        this.finalElapsed = this.durationMs;
        this.finished = true;
        this.reason = 'time';
      }
      return this.snapshot(now);
    }

    pause(now) {
      this.tick(now);
      if (this.started && !this.paused && !this.finished) {
        this.activeMs = this.elapsed(now);
        this.paused = true;
      }
      return this.snapshot(now);
    }

    resume(now) {
      if (this.started && this.paused && !this.finished) {
        this.paused = false;
        this.resumedAt = finite(now);
      }
      return this.snapshot(now);
    }

    // Long imported texts and stories can take a while. Allow a user to finish
    // an active session early and keep only its anonymous summary.
    finishEarly(now) {
      this.tick(now);
      if (!this.finished && this.started && (this.kind === 'story' || this.kind === 'import')) {
        this.finalElapsed = this.elapsed(now);
        this.finished = true;
        this.reason = 'stopped';
        this.paused = false;
      }
      return this.snapshot(now);
    }

    input(value, now) {
      this.tick(now);
      if (this.finished || this.paused) return this.snapshot(now);
      const next = String(value).replace(/\r\n?/g, '\n').slice(0, this.target.length);
      if (next === this.typed) return this.snapshot(now);
      const change = insertedRange(this.typed, next);
      if (change.text.length && !this.started) {
        this.started = true;
        this.resumedAt = finite(now);
      }
      for (let i = 0; i < change.text.length; i += 1) {
        this.attempts += 1;
        const expected = this.target[change.start + i];
        const wrong = change.text[i] !== expected;
        if (wrong) this.mistakes += 1;
        // Never derive or save per-letter data from personal passages or files.
        const letter = expected && expected.toLowerCase();
        if (!PRIVATE_KINDS.has(this.kind) && letter && /^[a-z]$/.test(letter)) {
          const counts = this.keyStats[letter] || { attempts: 0, mistakes: 0 };
          counts.attempts += 1;
          if (wrong) counts.mistakes += 1;
          this.keyStats[letter] = counts;
        }
      }
      this.typed = next;
      if (this.typed.length === this.target.length) {
        if (UNTIMED.has(this.kind)) {
          this.finalElapsed = this.elapsed(now);
          this.finished = true;
          this.reason = 'complete';
        } else {
          this.finishedCorrect += correctPositions(this.target, this.typed);
          this.finishedIncorrect += this.typed.length - correctPositions(this.target, this.typed);
          this.finishedWords += correctWords(this.target, this.typed);
          this.index = (this.index + 1) % this.texts.length;
          this.typed = '';
        }
      }
      return this.snapshot(now);
    }

    snapshot(now) {
      const elapsedMs = this.elapsed(now);
      // Timer ticks should not rescan a long passage when nothing was typed.
      if (this.scoredTarget !== this.target || this.scoredTyped !== this.typed) {
        this.scoredTarget = this.target;
        this.scoredTyped = this.typed;
        this.scoredCorrect = correctPositions(this.target, this.typed);
        this.scoredWords = correctWords(this.target, this.typed);
      }
      const correct = this.finishedCorrect + this.scoredCorrect;
      const currentIncorrect = this.typed.length - this.scoredCorrect;
      const accuracy = this.attempts ? Math.round((this.attempts - this.mistakes) * 100 / this.attempts) : null;
      const wpm = elapsedMs >= 1000 ? Math.round(correct / 5 / (elapsedMs / 60000)) : 0;
      return {
        kind: this.kind, target: this.target, typed: this.typed, index: this.index,
        started: this.started, paused: this.paused, finished: this.finished, reason: this.reason,
        elapsedMs, remainingMs: this.durationMs === null ? null : Math.max(0, this.durationMs - elapsedMs),
        attempts: this.attempts, mistakes: this.mistakes, correct,
        uncorrected: this.finishedIncorrect + currentIncorrect,
        words: this.finishedWords + this.scoredWords,
        accuracy, wpm,
        keyStats: Object.fromEntries(Object.entries(this.keyStats).map(([key, counts]) => [key, { ...counts }])),
      };
    }
  }

  function localDay(value = new Date()) {
    const date = value instanceof Date ? value : new Date(value);
    if (!Number.isFinite(date.getTime())) return '';
    return `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, '0')}-${String(date.getDate()).padStart(2, '0')}`;
  }

  function dayNumber(value) {
    if (!/^\d{4}-\d{2}-\d{2}$/.test(value)) return null;
    const [year, month, day] = value.split('-').map(Number);
    const utc = Date.UTC(year, month - 1, day);
    const check = new Date(utc);
    if (check.getUTCFullYear() !== year || check.getUTCMonth() !== month - 1 || check.getUTCDate() !== day) return null;
    return Math.floor(utc / 86400000);
  }

  function defaultProfile() {
    return { version: 1, settings: { goalMinutes: 10, keyboard: true, practiceSize: 'standard' }, records: [], completedLessons: [] };
  }

  // Retain only bounded counts for known ASCII letters. Never retain a
  // passage, the keys actually typed, or personal-text letter statistics.
  function sanitizeKeyStats(value, attempts, mistakes) {
    if (!value || typeof value !== 'object' || Array.isArray(value)) return {};
    const result = {};
    let totalAttempts = 0;
    let totalMistakes = 0;
    for (const key of LETTERS) {
      const row = value[key];
      if (row === undefined) continue;
      if (!row || typeof row !== 'object' || !Number.isSafeInteger(row.attempts) ||
          !Number.isSafeInteger(row.mistakes) || row.attempts < 1 || row.mistakes < 0 ||
          row.mistakes > row.attempts) return {};
      totalAttempts += row.attempts;
      totalMistakes += row.mistakes;
      if (totalAttempts > attempts || totalMistakes > mistakes) return {};
      result[key] = { attempts: row.attempts, mistakes: row.mistakes };
    }
    return result;
  }

  function validLessonId(value) { return typeof value === 'string' && /^[a-z0-9-]{1,60}$/.test(value); }

  function sanitizeProfile(value) {
    const profile = defaultProfile();
    if (!value || typeof value !== 'object') return profile;
    const settings = value.settings && typeof value.settings === 'object' ? value.settings : {};
    profile.settings.goalMinutes = GOALS.includes(settings.goalMinutes) ? settings.goalMinutes : 10;
    profile.settings.keyboard = typeof settings.keyboard === 'boolean' ? settings.keyboard : true;
    profile.settings.practiceSize = PRACTICE_SIZES.includes(settings.practiceSize) ? settings.practiceSize : 'standard';
    const savedCompleted = Array.isArray(value.completedLessons) ? value.completedLessons.slice(-MAX_COMPLETED_LESSONS).filter(validLessonId) : [];
    profile.completedLessons = [...new Set(savedCompleted)];
    if (!Array.isArray(value.records)) return profile;
    profile.records = value.records.slice(-MAX_RECORDS).filter((item) =>
      item && typeof item === 'object' && KINDS.has(item.kind) && dayNumber(item.day) !== null &&
      Number.isFinite(item.wpm) && Number.isFinite(item.accuracy) && Number.isFinite(item.durationMs) &&
      Number.isFinite(item.attempts) && Number.isFinite(item.mistakes) &&
      item.wpm >= 0 && item.accuracy >= 0 && item.accuracy <= 100 && item.durationMs >= 0 &&
      item.attempts >= 0 && item.mistakes >= 0 && item.mistakes <= item.attempts
    ).map((item) => {
      const attempts = clamp(Math.round(item.attempts), 0, 1000000);
      const mistakes = clamp(Math.round(item.mistakes), 0, attempts);
      return {
        kind: item.kind, day: item.day,
        lessonId: item.kind === 'lesson' && validLessonId(item.lessonId) ? item.lessonId : '',
        wpm: clamp(Math.round(item.wpm), 0, 9999), accuracy: clamp(Math.round(item.accuracy), 0, 100),
        durationMs: clamp(Math.round(item.durationMs), 0, 5 * 60 * 60 * 1000),
        attempts, mistakes,
        words: clamp(Math.round(finite(item.words)), 0, 1000000),
        uncorrected: clamp(Math.round(finite(item.uncorrected)), 0, 1000000),
        keyStats: PRIVATE_KINDS.has(item.kind) ? {} : sanitizeKeyStats(item.keyStats, attempts, mistakes),
      };
    });
    // Import previous versions' completed lessons from their saved records.
    const completed = new Set(profile.completedLessons);
    for (const record of profile.records) if (record.kind === 'lesson' && record.lessonId) completed.add(record.lessonId);
    profile.completedLessons = [...completed].slice(-MAX_COMPLETED_LESSONS);
    return profile;
  }

  function addRecord(profile, result, date = new Date(), lessonId = '') {
    const cleaned = sanitizeProfile(profile);
    if (!result || !result.finished || !result.attempts) return cleaned;
    const raw = {
      kind: result.kind, day: localDay(date), lessonId,
      wpm: result.wpm, accuracy: result.accuracy, durationMs: result.elapsedMs,
      attempts: result.attempts, mistakes: result.mistakes, words: result.words,
      uncorrected: result.uncorrected,
      keyStats: PRIVATE_KINDS.has(result.kind) ? {} : result.keyStats,
    };
    return sanitizeProfile({ ...cleaned, records: [...cleaned.records, raw] });
  }

  function streak(records, today = new Date()) {
    const days = new Set(records.map((item) => dayNumber(item.day)).filter((day) => day !== null));
    let current = dayNumber(localDay(today));
    if (current === null || (!days.has(current) && !days.has(current - 1))) return 0;
    if (!days.has(current)) current -= 1;
    let count = 0;
    while (days.has(current)) { count += 1; current -= 1; }
    return count;
  }

  function completionCount(completedLessons, allowedIds) {
    return allowedIds instanceof Set ? completedLessons.filter((id) => allowedIds.has(id)).length : completedLessons.length;
  }

  function stats(profile, today = new Date(), allowedLessonIds = null) {
    const cleaned = sanitizeProfile(profile);
    const records = cleaned.records;
    const attempts = records.reduce((sum, item) => sum + item.attempts, 0);
    const mistakes = records.reduce((sum, item) => sum + item.mistakes, 0);
    const todayKey = localDay(today);
    const todayMs = records.filter((item) => item.day === todayKey).reduce((sum, item) => sum + item.durationMs, 0);
    return {
      sessions: records.length,
      bestWpm: records.length ? Math.max(...records.map((item) => item.wpm)) : null,
      accuracy: attempts ? Math.round((attempts - mistakes) * 100 / attempts) : null,
      lessonsCompleted: completionCount(cleaned.completedLessons, allowedLessonIds),
      todayMs, totalMs: records.reduce((sum, item) => sum + item.durationMs, 0),
      streak: streak(records, today),
    };
  }

  function weakKeys(profile) {
    const totals = {};
    for (const record of sanitizeProfile(profile).records) {
      for (const [key, counts] of Object.entries(record.keyStats)) {
        const total = totals[key] || { key, attempts: 0, mistakes: 0 };
        total.attempts += counts.attempts;
        total.mistakes += counts.mistakes;
        totals[key] = total;
      }
    }
    return Object.values(totals).filter((row) => row.mistakes > 0)
      .map((row) => ({ ...row, errorRate: Math.round(row.mistakes * 100 / row.attempts) }))
      .sort((a, b) => (b.mistakes / b.attempts) - (a.mistakes / a.attempts) || b.mistakes - a.mistakes || a.key.localeCompare(b.key));
  }

  function longestStreak(records) {
    const days = [...new Set(records.map((item) => dayNumber(item.day)).filter((day) => day !== null))].sort((a, b) => a - b);
    let longest = 0;
    let current = 0;
    let previous = null;
    for (const day of days) {
      current = previous !== null && day === previous + 1 ? current + 1 : 1;
      longest = Math.max(longest, current);
      previous = day;
    }
    return longest;
  }

  // Lesson milestones use durable completions; other milestones use recent
  // history. Earlier profiles automatically import IDs from their records.
  function milestones(profile, allowedLessonIds = null) {
    const cleaned = sanitizeProfile(profile);
    const records = cleaned.records;
    const uniqueLessons = completionCount(cleaned.completedLessons, allowedLessonIds);
    const definitions = [
      ['first', 'First bloom', 'Complete your first practice session.', records.length, 1, 'leaf'],
      ['lessons', 'Lesson explorer', 'Practice five different lessons.', uniqueLessons, 5, 'book'],
      ['accuracy', 'Careful hands', 'Finish a session at 95% accuracy or better (20+ attempts).', Number(records.some((row) => row.attempts >= 20 && row.accuracy >= 95)), 1, 'target'],
      ['speed', 'Finding your flow', 'Reach 30 WPM in a session of at least 10 seconds.', Number(records.some((row) => row.wpm >= 30 && row.durationMs >= 10000)), 1, 'bolt'],
      ['focus', 'Practice with purpose', 'Complete a focused key drill.', Number(records.some((row) => row.kind === 'focus')), 1, 'keyboard'],
      ['streak', 'Three-day rhythm', 'Practice on three consecutive days.', longestStreak(records), 3, 'flame'],
    ];
    return definitions.map(([id, title, description, value, target, icon]) => ({
      id, title, description, current: Math.min(value, target), target, icon, earned: value >= target,
    }));
  }

  function recommendedLesson(lessons, profile) {
    const completed = new Set(sanitizeProfile(profile).completedLessons);
    return lessons.find((item) => !completed.has(item.id)) || lessons[0] || null;
  }

  return { STORAGE_KEY, MAX_RECORDS, MAX_COMPLETED_LESSONS, GOALS, PRACTICE_SIZES, CUSTOM_MIN, CUSTOM_MAX, IMPORT_MAX_BYTES, IMPORT_MAX_CHARS, LETTERS,
    TypingRun, prepareCustomText, prepareImportedText, correctPositions, correctWords, insertedRange, sanitizeKeyStats,
    localDay, dayNumber, defaultProfile, sanitizeProfile, addRecord, streak, longestStreak, stats, weakKeys, milestones, recommendedLesson };
});
