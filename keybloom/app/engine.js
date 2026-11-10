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
  const GOALS = [5, 10, 15, 20];
  const KINDS = new Set(['lesson', 'test', 'sprint']);
  const clamp = (value, min, max) => Math.max(min, Math.min(max, value));
  const finite = (value, fallback = 0) => Number.isFinite(Number(value)) ? Number(value) : fallback;

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
      if (end <= typed.length && typed.slice(match.index, end) === match[0]) count += 1;
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
      if (kind !== 'lesson' && (!Number.isFinite(durationMs) || durationMs <= 0)) {
        throw new Error('A timed run needs a positive duration.');
      }
      this.texts = texts.slice();
      this.kind = kind;
      this.durationMs = kind === 'lesson' ? null : durationMs;
      this.index = 0;
      this.typed = '';
      this.attempts = 0;
      this.mistakes = 0;
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
        if (change.text[i] !== this.target[change.start + i]) this.mistakes += 1;
      }
      this.typed = next;
      if (this.typed.length === this.target.length) {
        if (this.kind === 'lesson') {
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
      const correct = this.finishedCorrect + correctPositions(this.target, this.typed);
      const currentIncorrect = this.typed.length - correctPositions(this.target, this.typed);
      const accuracy = this.attempts ? Math.round((this.attempts - this.mistakes) * 100 / this.attempts) : null;
      const wpm = elapsedMs >= 1000 ? Math.round(correct / 5 / (elapsedMs / 60000)) : 0;
      return {
        kind: this.kind, target: this.target, typed: this.typed, index: this.index,
        started: this.started, paused: this.paused, finished: this.finished, reason: this.reason,
        elapsedMs, remainingMs: this.durationMs === null ? null : Math.max(0, this.durationMs - elapsedMs),
        attempts: this.attempts, mistakes: this.mistakes, correct,
        uncorrected: this.finishedIncorrect + currentIncorrect,
        words: this.finishedWords + correctWords(this.target, this.typed),
        accuracy, wpm,
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
    return { version: 1, settings: { goalMinutes: 10, keyboard: true }, records: [] };
  }

  function sanitizeProfile(value) {
    const profile = defaultProfile();
    if (!value || typeof value !== 'object') return profile;
    const settings = value.settings && typeof value.settings === 'object' ? value.settings : {};
    profile.settings.goalMinutes = GOALS.includes(settings.goalMinutes) ? settings.goalMinutes : 10;
    profile.settings.keyboard = typeof settings.keyboard === 'boolean' ? settings.keyboard : true;
    if (!Array.isArray(value.records)) return profile;
    profile.records = value.records.slice(-MAX_RECORDS).filter((item) =>
      item && typeof item === 'object' && KINDS.has(item.kind) && dayNumber(item.day) !== null &&
      Number.isFinite(item.wpm) && Number.isFinite(item.accuracy) && Number.isFinite(item.durationMs) &&
      Number.isFinite(item.attempts) && Number.isFinite(item.mistakes) &&
      item.wpm >= 0 && item.accuracy >= 0 && item.accuracy <= 100 && item.durationMs >= 0 &&
      item.attempts >= 0 && item.mistakes >= 0 && item.mistakes <= item.attempts
    ).map((item) => ({
      kind: item.kind, day: item.day,
      lessonId: typeof item.lessonId === 'string' && /^[a-z0-9-]{1,60}$/.test(item.lessonId) ? item.lessonId : '',
      wpm: clamp(Math.round(item.wpm), 0, 9999), accuracy: clamp(Math.round(item.accuracy), 0, 100),
      durationMs: clamp(Math.round(item.durationMs), 0, 5 * 60 * 60 * 1000),
      attempts: clamp(Math.round(item.attempts), 0, 1000000),
      mistakes: clamp(Math.round(item.mistakes), 0, 1000000),
      words: clamp(Math.round(finite(item.words)), 0, 1000000),
      uncorrected: clamp(Math.round(finite(item.uncorrected)), 0, 1000000),
    }));
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

  function stats(profile, today = new Date()) {
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
      lessonsCompleted: new Set(records.filter((item) => item.kind === 'lesson' && item.lessonId).map((item) => item.lessonId)).size,
      todayMs, totalMs: records.reduce((sum, item) => sum + item.durationMs, 0),
      streak: streak(records, today),
    };
  }

  function recommendedLesson(lessons, profile) {
    const records = sanitizeProfile(profile).records;
    const completed = new Set(records.filter((item) => item.kind === 'lesson').map((item) => item.lessonId));
    return lessons.find((item) => !completed.has(item.id)) || lessons[0] || null;
  }

  return { STORAGE_KEY, MAX_RECORDS, GOALS, TypingRun, correctPositions, correctWords, insertedRange,
    localDay, dayNumber, defaultProfile, sanitizeProfile, addRecord, streak, stats, recommendedLesson };
});
