/*
 * KeyBloom — original, offline typing studio.
 * Copyright (C) 2026 Emmanuel Suah
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
(() => {
  'use strict';
  const E = window.KeyBloomEngine;
  const C = window.KeyBloomContent;
  const $ = (id) => document.getElementById(id);
  const icon = (name) => `<svg aria-hidden="true"><use href="#i-${name}"></use></svg>`;
  const esc = (value) => String(value).replace(/[&<>"']/g, (char) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[char]));
  const ui = {
    profile: E.defaultProfile(), screen: 'home', run: null, runInfo: null, timer: null,
    storageWarning: '', testSeconds: 60, dialogAction: null, dialogCancel: null, result: null, toastTimer: null,
  };

  function loadProfile() {
    let raw;
    try { raw = window.localStorage.getItem(E.STORAGE_KEY); }
    catch (error) {
      ui.storageWarning = 'Progress cannot be loaded or saved on this device. Practice still works for this session.';
      return E.defaultProfile();
    }
    if (!raw) return E.defaultProfile();
    try { return E.sanitizeProfile(JSON.parse(raw)); }
    catch (error) {
      ui.storageWarning = 'Saved progress could not be read, so KeyBloom is starting fresh. New activity will replace the unreadable data.';
      return E.defaultProfile();
    }
  }

  function persist() {
    try {
      window.localStorage.setItem(E.STORAGE_KEY, JSON.stringify(ui.profile));
      return true;
    } catch (error) {
      ui.storageWarning = 'Progress could not be saved. Your practice will be available only until this window closes.';
      updateWarning();
      return false;
    }
  }

  function updateWarning() {
    const node = $('storage-warning');
    node.hidden = !ui.storageWarning;
    node.textContent = ui.storageWarning;
  }

  function toast(message) {
    const node = $('toast');
    node.textContent = message;
    node.hidden = false;
    clearTimeout(ui.toastTimer);
    ui.toastTimer = setTimeout(() => { node.hidden = true; }, 3500);
  }

  function pageTop(kicker, title, subtitle) {
    const summary = E.stats(ui.profile);
    return `<div class="page-top"><div><div class="eyebrow">${esc(kicker)}</div><h1>${esc(title)}</h1><p class="page-subtitle">${esc(subtitle)}</p></div>
      <div class="top-right"><div class="streak-chip">${icon('flame')} ${summary.streak} day${summary.streak === 1 ? '' : 's'} in a row</div><div class="avatar" aria-hidden="true">${icon('leaf')}</div></div></div>`;
  }

  function pageIntro(kicker, title, subtitle, badge = '') {
    return `<div class="page-intro"><div><div class="eyebrow">${esc(kicker)}</div><h1>${esc(title)}</h1><p>${esc(subtitle)}</p></div>${badge ? `<span class="quiet-pill">${esc(badge)}</span>` : ''}</div>`;
  }

  function statCard(symbol, tone, label, value, unit) {
    return `<div class="stat-card"><div class="stat-icon ${esc(tone)}">${icon(symbol)}</div><div><div class="stat-label">${esc(label)}</div><div class="stat-value">${value === null ? '—' : esc(value)}${unit ? `<small>${esc(unit)}</small>` : ''}</div></div></div>`;
  }

  function lessonRecords(id) { return ui.profile.records.filter((row) => row.kind === 'lesson' && row.lessonId === id); }
  function isDone(id) { return lessonRecords(id).length > 0; }
  function lessonById(id) { return C.lessons.find((item) => item.id === id); }
  function lessonNum(id) { return C.lessons.findIndex((item) => item.id === id) + 1; }
  function minutes(ms) { return ms <= 0 ? '0' : (ms < 3000 ? '<0.1' : ms < 60000 ? (ms / 60000).toFixed(1) : String(Math.round(ms / 60000))); }

  function renderHome() {
    const summary = E.stats(ui.profile);
    const next = E.recommendedLesson(C.lessons, ui.profile);
    const nextIndex = Math.max(0, C.lessons.findIndex((lesson) => lesson.id === next.id));
    const sample = C.lessons.slice(nextIndex, nextIndex + 3);
    const goal = ui.profile.settings.goalMinutes;
    const percent = Math.min(100, Math.round(summary.todayMs / (goal * 60000) * 100));
    const radius = 38;
    const circumference = 2 * Math.PI * radius;
    const rows = sample.map((lesson) => `<div class="path-row"><div class="path-number ${isDone(lesson.id) ? 'done' : ''}">${isDone(lesson.id) ? icon('check') : String(lessonNum(lesson.id)).padStart(2, '0')}</div>
      <div class="path-copy"><div class="path-title">${esc(lesson.title)}</div><div class="path-meta">${esc(lesson.group)} · ${lesson.minutes} min · ${esc(lesson.keys)}</div></div>
      <button class="path-go" data-action="lesson" data-id="${esc(lesson.id)}" aria-label="Start ${esc(lesson.title)}">${icon('arrow')}</button></div>`).join('');
    return `${pageTop('Your typing studio', 'Good to see you.', 'A little practice today, a little more confidence tomorrow.')}
      <section class="hero" aria-label="Start learning"><div class="hero-art" aria-hidden="true"></div><div class="hero-copy"><div class="eyebrow">A LITTLE BETTER EVERY DAY</div>
        <h2>Better typing,<br><em>one key at a time.</em></h2><p>Build confidence at your own pace with friendly lessons, helpful feedback, and a little room to play.</p>
        <div class="hero-actions"><button class="btn btn-dark" data-action="lesson" data-id="${esc(next.id)}">${summary.lessonsCompleted ? 'Continue learning' : 'Start your first lesson'} ${icon('arrow')}</button><span class="mini-note">${icon('clock')} About ${next.minutes} min</span></div></div></section>
      <div class="stats-grid" aria-label="Your highlights">${statCard('bolt', '', 'Best speed', summary.bestWpm, 'WPM')}${statCard('target', 'peach', 'Average accuracy', summary.accuracy, '%')}${statCard('book', 'lavender', 'Lessons practiced', summary.lessonsCompleted, `/ ${C.lessons.length}`)}</div>
      <div class="home-grid"><section><div class="section-head"><div><h2>Your learning path</h2><p>A comfortable place to begin, wherever you are.</p></div><button class="btn btn-text" data-action="navigate" data-screen="lessons">View all ${icon('arrow')}</button></div><div class="card path-card">${rows}</div></section>
        <section><div class="section-head"><div><h2>A little every day</h2><p>Today's practice goal</p></div></div><div class="card goal-card"><div class="goal-ring"><svg viewBox="0 0 100 100" aria-hidden="true"><circle class="ring-bg" cx="50" cy="50" r="38"/><circle class="ring-fill" cx="50" cy="50" r="38" stroke-dasharray="${(circumference * percent / 100).toFixed(1)} ${circumference.toFixed(1)}"/></svg><div class="goal-ring-text">${percent}%</div></div><div class="goal-copy"><div class="eyebrow">YOUR DAILY GOAL</div><h3>${esc(minutes(summary.todayMs))} of ${goal} min</h3><p>Completed sessions count toward your goal.</p><div class="goal-note">${icon('spark')} Every minute matters</div></div></div></section></div>`;
  }

  function renderLessons() {
    const summary = E.stats(ui.profile);
    const groups = [...new Set(C.lessons.map((item) => item.group))];
    const tone = ['','peach','lavender','sky'];
    const sections = groups.map((group, groupIndex) => {
      const lessons = C.lessons.filter((item) => item.group === group);
      return `<section class="course-section"><div class="section-head"><div><div class="eyebrow">CHAPTER ${String(groupIndex + 1).padStart(2, '0')}</div><h2>${esc(group)}</h2></div><span class="quiet-pill">${lessons.length} lessons</span></div>
        <div class="lesson-grid">${lessons.map((lesson) => {
          const previous = lessonRecords(lesson.id);
          const last = previous[previous.length - 1];
          return `<article class="card lesson-card"><div class="lesson-top"><span class="lesson-badge ${tone[groupIndex] || ''}">${esc(lesson.keys)}</span><span class="lesson-number">${String(lessonNum(lesson.id)).padStart(2, '0')}</span></div>
            <h3>${esc(lesson.title)}</h3><p>About ${lesson.minutes} min · ${esc(group)}</p><div class="lesson-bottom"><span class="lesson-status">${last ? `✓ Practiced · ${last.accuracy}% accuracy` : 'Ready when you are'}</span>
            <button class="lesson-start" data-action="lesson" data-id="${esc(lesson.id)}" aria-label="${last ? 'Practice again:' : 'Start:'} ${esc(lesson.title)}">${last ? 'Practice again' : 'Start lesson'} ${icon('arrow')}</button></div></article>`;
        }).join('')}</div></section>`;
    }).join('');
    return `${pageIntro('THE LEARNING PATH', 'Make yourself at home.', 'Start with the home row and build up to everyday sentences. Every lesson is available to revisit whenever you like.', `${summary.lessonsCompleted} / ${C.lessons.length} practiced`)}${sections}`;
  }

  function renderPractice() {
    return `${pageIntro('PRACTICE YOUR WAY', 'Find your flow.', 'A little challenge is a lovely way to see how far you have come. The timer starts when you type.')}
      <div class="practice-grid"><section class="card challenge-card"><div class="challenge-icon">${icon('clock')}</div><h2>Typing test</h2><p>Find a steady rhythm with a timed collection of short, original passages. Focus on accuracy and let your speed follow.</p>
      <label class="select-label" for="test-duration">CHOOSE A TIME <select id="test-duration"><option value="60" ${ui.testSeconds === 60 ? 'selected' : ''}>1 minute</option><option value="180" ${ui.testSeconds === 180 ? 'selected' : ''}>3 minutes</option></select></label>
      <button class="btn btn-dark" data-action="start-test">Start a typing test ${icon('arrow')}</button></section>
      <section class="card challenge-card"><div class="challenge-icon peach">${icon('spark')}</div><h2>Word sprint</h2><p>How many words can you type correctly in 45 seconds? A light-hearted game to build rhythm, one word at a time.</p>
      <div class="mini-note">${icon('clock')} 45 seconds · no punctuation</div><button class="btn btn-dark" data-action="start-sprint">Play word sprint ${icon('arrow')}</button></section></div>
      <div class="tip-strip">${icon('leaf')} Your progress is personal. You can retry any activity as often as you like.</div>`;
  }

  function dateLabel(day) {
    const parts = day.split('-').map(Number);
    const date = new Date(parts[0], parts[1] - 1, parts[2]);
    return Number.isFinite(date.getTime()) ? date.toLocaleDateString(undefined, { month: 'short', day: 'numeric' }) : day;
  }

  function chartMarkup(records) {
    const values = records.slice(-8).map((item) => item.wpm);
    if (!values.length) return '<div class="chart-empty">Complete a session to see your speed trend.</div>';
    const high = Math.max(20, ...values) * 1.12;
    const coords = values.map((v, i) => ({ x: values.length === 1 ? 20 : 20 + (i / (values.length - 1)) * 560, y: 132 - (v / high) * 108 }));
    return `<svg class="chart" viewBox="0 0 600 150" role="img" aria-label="Recent typing speeds, in words per minute: ${values.join(', ')}"><path class="grid" d="M20 25H580 M20 60H580 M20 95H580 M20 132H580"/><polyline class="line" points="${coords.map((p) => `${p.x.toFixed(1)},${p.y.toFixed(1)}`).join(' ')}"/>${coords.map((p) => `<circle class="dot" cx="${p.x.toFixed(1)}" cy="${p.y.toFixed(1)}" r="4"/>`).join('')}</svg>`;
  }

  function renderProgress() {
    const summary = E.stats(ui.profile);
    const history = ui.profile.records.slice(-6).reverse();
    const historyMarkup = history.length ? history.map((row) => {
      const lesson = lessonById(row.lessonId);
      const title = row.kind === 'lesson' ? (lesson ? lesson.title : 'Lesson') : row.kind === 'sprint' ? 'Word sprint' : 'Typing test';
      return `<div class="history-row"><div><div class="history-title">${esc(title)}</div><div class="history-meta">${esc(dateLabel(row.day))} · ${row.accuracy}% accuracy</div></div><span class="history-score">${row.wpm} WPM</span></div>`;
    }).join('') : '<div class="empty-history">Your completed lessons and practice sessions will appear here.</div>';
    return `${pageIntro('YOUR PERSONAL PROGRESS', 'Look how far you\'ve come.', 'This is your space to notice the small improvements. All progress stays on this device.', `${summary.sessions} session${summary.sessions === 1 ? '' : 's'}`)}
      <div class="stats-grid">${statCard('bolt', '', 'Best speed', summary.bestWpm, 'WPM')}${statCard('target', 'peach', 'Average accuracy', summary.accuracy, '%')}${statCard('clock', 'lavender', 'Time practiced', minutes(summary.totalMs), 'min')}</div>
      <div class="progress-grid"><section class="card chart-card"><h2>Speed over time</h2><p>Your most recent sessions, in words per minute.</p>${chartMarkup(ui.profile.records)}</section>
      <section class="card history-card"><h2>Recent sessions</h2><p>Your latest steps forward.</p><div class="history-list">${historyMarkup}</div></section></div>`;
  }

  function renderSettings() {
    return `${pageIntro('MAKE IT YOURS', 'A comfortable way to learn.', 'Adjust your practice space. Your settings and progress live only on this computer.')}
      <div class="settings-layout"><section class="card settings-card"><h2>Your preferences</h2><p>Make a little room for the way you like to practice.</p>
      <div class="setting-row"><div><strong>Daily practice goal</strong><p>Completed sessions count toward this goal.</p></div><label for="goal-select" class="sr-only">Daily practice goal in minutes</label><select id="goal-select">${E.GOALS.map((goal) => `<option value="${goal}" ${goal === ui.profile.settings.goalMinutes ? 'selected' : ''}>${goal} minutes</option>`).join('')}</select></div>
      <div class="setting-row"><div><strong>On-screen keyboard</strong><p>Show the next key and a finger hint during practice.</p></div><label class="switch"><input type="checkbox" id="keyboard-setting" ${ui.profile.settings.keyboard ? 'checked' : ''}> Show</label></div>
      <div class="setting-row"><div><strong>Reset this device</strong><p>Remove all saved sessions and preferences. This cannot be undone.</p></div></div><button class="btn btn-danger" data-action="reset">Reset progress</button></section>
      <aside class="privacy-box">${icon('shield')}<h3>Just for you.</h3><p>No account, tracking, or internet connection is needed. KeyBloom stores session summaries on this device, not the text you type.</p><small>KeyBloom · made by Emmanuel Suah</small></aside></div>`;
  }

  const rows = [
    [ ['`','`'], ['1','1'], ['2','2'], ['3','3'], ['4','4'], ['5','5'], ['6','6'], ['7','7'], ['8','8'], ['9','9'], ['0','0'], ['-','-'], ['=','='], ['back','⌫','wide'] ],
    [ ['tab','Tab','wide'], ['q','Q'], ['w','W'], ['e','E'], ['r','R'], ['t','T'], ['y','Y'], ['u','U'], ['i','I'], ['o','O'], ['p','P'], ['[','['], [']',']'], ['\\','\\'] ],
    [ ['caps','Caps','wide'], ['a','A'], ['s','S'], ['d','D'], ['f','F'], ['g','G'], ['h','H'], ['j','J'], ['k','K'], ['l','L'], [';',';'], ["'","'"], ['enter','Enter','wide'] ],
    [ ['shift-left','Shift','shift'], ['z','Z'], ['x','X'], ['c','C'], ['v','V'], ['b','B'], ['n','N'], ['m','M'], [',',','], ['.','.'], ['/','/'], ['shift-right','Shift','shift'] ],
    [ ['ctrl','Ctrl','wide'], ['win','Win','wide'], ['alt','Alt','wide'], [' ','Space','space'], ['alt2','Alt','wide'], ['fn','Fn','wide'], ['ctrl2','Ctrl','wide'] ],
  ];
  const zones = ["`1qaz", '2wsx', '3edc', '45rtfgvb', '67yuhjnm', '8ik,', '9ol.', "0-p;/'[=]\\"];
  const fingers = ['Left little finger', 'Left ring finger', 'Left middle finger', 'Left index finger', 'Right index finger', 'Right middle finger', 'Right ring finger', 'Right little finger'];
  const shifted = { '!':'1', '@':'2', '#':'3', '$':'4', '%':'5', '^':'6', '&':'7', '*':'8', '(':'9', ')':'0', '_':'-', '+':'=', '{':'[', '}':']', '|':'\\', ':':';', '"':"'", '<':',', '>':'.', '?':'/', '~':'`' };

  function keyInfo(character) {
    if (!character) return { key: '', shift: false, hint: 'Keep going at your own pace' };
    const shift = Object.prototype.hasOwnProperty.call(shifted, character) || /^[A-Z]$/.test(character);
    const key = shifted[character] || character.toLowerCase();
    const index = zones.findIndex((zone) => zone.includes(key));
    const finger = key === ' ' ? 'Either thumb' : index >= 0 ? fingers[index] : 'Follow the highlighted key';
    return { key, shift, hint: `${shift ? 'Hold Shift · ' : ''}${finger} · ${character === ' ' ? 'Space' : character}` };
  }

  function keyboardMarkup() {
    return rows.map((row) => `<div class="key-row">${row.map(([key, label, size]) => {
      const zone = zones.findIndex((group) => group.includes(key));
      return `<div class="key ${esc(size || '')} ${zone >= 0 ? `zone-${zone % 4 + 1}` : ''}" data-key="${esc(key)}">${esc(label)}</div>`;
    }).join('')}</div>`).join('');
  }

  function clock(ms, up = false) {
    const seconds = Math.max(0, up ? Math.floor(ms / 1000) : Math.ceil(ms / 1000));
    return `${Math.floor(seconds / 60)}:${String(seconds % 60).padStart(2, '0')}`;
  }

  function currentSnapshot() { return ui.run.snapshot(performance.now()); }
  function selectedScreen() {
    if (ui.screen === 'session' || ui.screen === 'result') return ui.runInfo && ui.runInfo.kind === 'lesson' ? 'lessons' : 'practice';
    return ui.screen;
  }

  function renderSession() {
    const info = ui.runInfo;
    const snap = currentSnapshot();
    const lesson = info.kind === 'lesson' ? lessonById(info.lessonId) : null;
    const title = lesson ? lesson.title : info.kind === 'sprint' ? 'Word sprint' : 'Typing test';
    const group = lesson ? `LESSON ${String(lessonNum(lesson.id)).padStart(2, '0')} · ${lesson.group}` : info.kind === 'sprint' ? 'THE 45-SECOND GAME' : `${info.seconds / 60}-MINUTE PRACTICE`;
    const description = lesson ? lesson.tip : info.kind === 'sprint' ? 'Type as many words correctly as you can. The timer starts with your first key.' : 'Type at a comfortable pace. The timer starts with your first key.';
    return `<div class="run-top"><button class="back-link" data-action="navigate" data-screen="${lesson ? 'lessons' : 'practice'}">${icon('back')} Back to ${lesson ? 'lessons' : 'practice'}</button><span class="quiet-pill">${lesson ? `${lessonNum(lesson.id)} of ${C.lessons.length} lessons` : 'Timed practice'}</span></div>
      <div class="run-heading"><div><div class="eyebrow">${esc(group)}</div><h1>${esc(title)}</h1><p>${esc(description)}</p></div><button class="btn btn-subtle" data-action="toggle-keyboard" id="keyboard-toggle">${icon('keyboard')} ${ui.profile.settings.keyboard ? 'Hide keyboard' : 'Show keyboard'}</button></div>
      <div class="run-stats"><div class="run-stat"><div class="eyebrow">SPEED</div><strong><span id="run-wpm">—</span> <small>WPM</small></strong></div>
        <div class="run-stat"><div class="eyebrow">ACCURACY</div><strong><span id="run-accuracy">—</span><small>%</small></strong></div>
        <div class="run-stat"><div class="eyebrow">${snap.remainingMs === null ? 'ELAPSED' : 'TIME LEFT'}</div><strong id="run-time">${snap.remainingMs === null ? '0:00' : clock(snap.remainingMs)}</strong></div>
        ${info.kind === 'sprint' ? '<div class="run-stat"><div class="eyebrow">CORRECT WORDS</div><strong id="run-words">0</strong></div>' : ''}
        <div class="run-stat action"><button class="btn btn-light" data-action="pause" id="pause-button">${icon('pause')} Pause</button></div></div>
      <section class="card typing-card"><div class="typing-header"><div class="eyebrow">TYPE THE TEXT BELOW</div><span class="typing-count" id="typing-count"></span></div>
        <div class="target-text" id="target-text" aria-hidden="true"></div><p class="sr-only" id="target-accessible"></p>
        <label for="typing-input" class="input-label"><span>Your turn</span><span id="typing-state">Your timer starts when you type</span></label>
        <textarea id="typing-input" class="typing-input" rows="2" spellcheck="false" autocomplete="off" autocapitalize="off" aria-describedby="target-accessible typing-help" placeholder="Click here and start typing…"></textarea>
        <div class="typing-help" id="typing-help"><span>Backspace to correct. Pasting is disabled during practice.</span><span id="typing-status">You can pause whenever you need.</span></div><div class="run-message" id="run-message" role="status" aria-live="polite"></div></section>
      <section class="card keyboard-card" id="keyboard-card" ${ui.profile.settings.keyboard ? '' : 'hidden'}><div class="keyboard-head"><span class="eyebrow">YOUR KEYBOARD · US QWERTY</span><span class="keyboard-hint" id="keyboard-hint"></span></div><div class="keyboard" id="keyboard" aria-hidden="true">${keyboardMarkup()}</div></section>`;
  }

  function renderResult() {
    const result = ui.result;
    const info = ui.runInfo;
    const lesson = info.kind === 'lesson' ? lessonById(info.lessonId) : null;
    const next = lesson ? C.lessons[lessonNum(lesson.id)] : null;
    const advice = result.accuracy >= 95 ? 'Beautifully done. You kept a careful, steady rhythm.' : result.accuracy >= 80 ? 'Nice work. A steady pace can help those tricky keys feel easier.' : 'You showed up and practiced. Try slowing down and letting accuracy lead.';
    return `<div class="run-top"><button class="back-link" data-action="navigate" data-screen="${lesson ? 'lessons' : 'practice'}">${icon('back')} Back to ${lesson ? 'lessons' : 'practice'}</button></div><div class="result-wrap"><section class="card result-card">
      <div class="result-emblem">${icon('spark')}</div><div class="eyebrow">${info.kind === 'sprint' ? 'SPRINT COMPLETE' : lesson ? 'LESSON COMPLETE' : 'TEST COMPLETE'}</div><h1>A little better, already.</h1><p>${esc(advice)}</p>
      <div class="result-metrics"><div class="result-metric"><strong>${result.wpm}</strong><span>words per minute</span></div><div class="result-metric"><strong>${result.accuracy}%</strong><span>accuracy</span></div><div class="result-metric"><strong>${info.kind === 'sprint' ? result.words : clock(result.elapsedMs, true)}</strong><span>${info.kind === 'sprint' ? 'correct words' : 'time practiced'}</span></div></div>
      <div class="result-actions"><button class="btn btn-subtle" data-action="retry">${icon('refresh')} Try again</button>${next ? `<button class="btn btn-dark" data-action="lesson" data-id="${esc(next.id)}">Next lesson ${icon('arrow')}</button>` : `<button class="btn btn-dark" data-action="navigate" data-screen="progress">See your progress ${icon('arrow')}</button>`}</div>
      <div class="result-detail">${result.mistakes} mistake${result.mistakes === 1 ? '' : 's'} across ${result.attempts} character attempts; ${result.uncorrected} uncorrected. Corrected mistakes still count in accuracy.</div></section></div>`;
  }

  function render() {
    updateWarning();
    const page = { home: renderHome, lessons: renderLessons, practice: renderPractice, progress: renderProgress,
      settings: renderSettings, session: renderSession, result: renderResult }[ui.screen] || renderHome;
    $('view').innerHTML = page();
    for (const node of document.querySelectorAll('.nav-item')) {
      if (node.dataset.screen === selectedScreen()) node.setAttribute('aria-current', 'page');
      else node.removeAttribute('aria-current');
    }
    if (ui.screen === 'session') {
      const input = $('typing-input');
      input.maxLength = ui.run.target.length;
      input.value = ui.run.typed;
      updateLive(currentSnapshot(), true);
      requestAnimationFrame(() => { if (ui.screen === 'session' && !ui.run.paused) input.focus(); });
    }
  }

  function stopTimer() { if (ui.timer) { clearInterval(ui.timer); ui.timer = null; } }

  function navigate(screen, force = false) {
    if (!['home', 'lessons', 'practice', 'progress', 'settings'].includes(screen)) return;
    if (!force && ui.screen === 'session' && ui.run && ui.run.started && !ui.run.finished) {
      confirmDialog('Leave this practice?', 'Your current session is not saved until you finish it.', 'Leave practice', () => navigate(screen, true));
      return;
    }
    stopTimer();
    ui.run = null; ui.result = null; ui.runInfo = null;
    ui.screen = screen;
    $('workspace').scrollTop = 0;
    render();
  }

  function startRun(info) {
    const lesson = info.kind === 'lesson' ? lessonById(info.lessonId) : null;
    if (info.kind === 'lesson' && !lesson) return;
    stopTimer();
    ui.runInfo = { ...info };
    ui.run = new E.TypingRun({
      kind: info.kind,
      texts: lesson ? [lesson.text] : info.kind === 'sprint' ? C.sprintPassages : C.passages,
      durationMs: info.kind === 'lesson' ? null : info.seconds * 1000,
    });
    ui.result = null;
    ui.screen = 'session';
    $('workspace').scrollTop = 0;
    render();
    ui.timer = setInterval(() => {
      if (ui.screen !== 'session' || !ui.run) return;
      const snapshot = ui.run.tick(performance.now());
      if (snapshot.finished) finishRun(snapshot);
      else updateLive(snapshot, false);
    }, 200);
  }

  function finishRun(snapshot) {
    if (ui.screen !== 'session') return;
    stopTimer();
    ui.result = snapshot;
    ui.profile = E.addRecord(ui.profile, snapshot, new Date(), ui.runInfo.lessonId || '');
    if (snapshot.attempts) persist();
    ui.screen = 'result';
    $('workspace').scrollTop = 0;
    render();
    $('workspace').focus({ preventScroll: true });
  }

  function paintTarget(snapshot) {
    const input = $('typing-input');
    if (!input) return;
    const target = snapshot.target;
    const typed = snapshot.typed;
    const caret = document.activeElement === input ? input.selectionStart : typed.length;
    $('target-text').innerHTML = Array.from(target).map((char, index) => {
      const state = index < typed.length ? (typed[index] === char ? 'correct' : 'wrong') : '';
      const current = !snapshot.finished && index === caret ? 'current' : '';
      return `<span class="target-char ${state} ${current}">${esc(char)}</span>`;
    }).join('');
    $('target-accessible').textContent = `Practice text: ${target}`;
    const character = target[Math.min(caret, target.length - 1)];
    const hint = keyInfo(character);
    if ($('keyboard-hint')) $('keyboard-hint').textContent = hint.hint;
    if ($('keyboard')) for (const key of $('keyboard').querySelectorAll('.key')) {
      key.classList.toggle('next', key.dataset.key === hint.key);
      key.classList.toggle('shift-needed', hint.shift && key.dataset.key.startsWith('shift-'));
    }
  }

  function updateLive(snapshot, repaint) {
    if (ui.screen !== 'session' || !$('typing-input')) return;
    $('run-wpm').textContent = snapshot.started && snapshot.elapsedMs >= 1000 ? String(snapshot.wpm) : '—';
    $('run-accuracy').textContent = snapshot.accuracy === null ? '—' : String(snapshot.accuracy);
    $('run-time').textContent = snapshot.remainingMs === null ? clock(snapshot.elapsedMs, true) : clock(snapshot.remainingMs);
    if ($('run-words')) $('run-words').textContent = String(snapshot.words);
    $('typing-count').textContent = `${snapshot.typed.length} / ${snapshot.target.length}`;
    $('typing-state').textContent = snapshot.paused ? 'Practice paused' : snapshot.started ? 'Keep going — you are doing great' : 'Your timer starts when you type';
    $('typing-status').textContent = snapshot.paused ? 'Press Resume when you are ready.' : 'You can pause whenever you need.';
    $('pause-button').innerHTML = snapshot.paused ? `${icon('play')} Resume` : `${icon('pause')} Pause`;
    $('pause-button').disabled = !snapshot.started;
    const input = $('typing-input');
    input.readOnly = snapshot.paused;
    if (input.maxLength !== snapshot.target.length) input.maxLength = snapshot.target.length;
    if (input.value !== snapshot.typed) input.value = snapshot.typed;
    if (repaint) paintTarget(snapshot);
  }

  function confirmDialog(title, message, confirmLabel, action) {
    const running = ui.screen === 'session' && ui.run && ui.run.started && !ui.run.paused && !ui.run.finished ? ui.run : null;
    if (running) { running.pause(performance.now()); updateLive(running.snapshot(performance.now()), true); }
    ui.dialogAction = action;
    ui.dialogCancel = () => {
      if (running && ui.run === running && ui.screen === 'session') {
        running.resume(performance.now()); updateLive(running.snapshot(performance.now()), true);
        $('typing-input').focus();
      }
    };
    $('dialog-title').textContent = title;
    $('dialog-message').textContent = message;
    $('dialog-confirm').textContent = confirmLabel;
    $('confirm-dialog').showModal();
    $('dialog-cancel').focus();
  }

  function closeDialog(confirmed) {
    const action = confirmed ? ui.dialogAction : ui.dialogCancel;
    ui.dialogAction = null; ui.dialogCancel = null;
    $('confirm-dialog').close();
    if (typeof action === 'function') action();
  }

  document.addEventListener('click', (event) => {
    const button = event.target.closest('[data-action]');
    if (!button || button.disabled) return;
    const action = button.dataset.action;
    if (action === 'navigate') navigate(button.dataset.screen);
    else if (action === 'lesson') startRun({ kind: 'lesson', lessonId: button.dataset.id });
    else if (action === 'start-test') { ui.testSeconds = Number($('test-duration').value) === 180 ? 180 : 60; startRun({ kind: 'test', seconds: ui.testSeconds }); }
    else if (action === 'start-sprint') startRun({ kind: 'sprint', seconds: 45 });
    else if (action === 'retry' && ui.runInfo) startRun(ui.runInfo);
    else if (action === 'toggle-keyboard') {
      ui.profile.settings.keyboard = !ui.profile.settings.keyboard; persist();
      $('keyboard-card').hidden = !ui.profile.settings.keyboard;
      $('keyboard-toggle').innerHTML = `${icon('keyboard')} ${ui.profile.settings.keyboard ? 'Hide keyboard' : 'Show keyboard'}`;
      if (ui.profile.settings.keyboard) paintTarget(currentSnapshot());
      $('typing-input').focus();
    } else if (action === 'pause' && ui.run) {
      if (ui.run.paused) ui.run.resume(performance.now()); else ui.run.pause(performance.now());
      $('run-message').textContent = ui.run.paused ? 'Paused. Press Resume when you are ready.' : '';
      updateLive(currentSnapshot(), true);
      if (!ui.run.paused) $('typing-input').focus();
    } else if (action === 'reset') {
      confirmDialog('Reset all progress?', 'This removes your saved sessions and preferences from this device. This cannot be undone.', 'Reset progress', () => {
        ui.profile = E.defaultProfile(); ui.storageWarning = ''; persist(); render(); toast('Progress and settings reset on this device.');
      });
    }
  });

  document.addEventListener('input', (event) => {
    if (event.target.id !== 'typing-input' || ui.screen !== 'session' || !ui.run) return;
    const snapshot = ui.run.input(event.target.value, performance.now());
    if (snapshot.finished) finishRun(snapshot);
    else updateLive(snapshot, true);
  });
  document.addEventListener('change', (event) => {
    if (event.target.id === 'goal-select') {
      const value = Number(event.target.value);
      if (E.GOALS.includes(value)) { ui.profile.settings.goalMinutes = value; persist(); toast('Daily goal updated.'); }
    } else if (event.target.id === 'keyboard-setting') {
      ui.profile.settings.keyboard = event.target.checked; persist(); toast('Keyboard preference saved.');
    }
  });
  for (const name of ['paste', 'drop']) document.addEventListener(name, (event) => {
    if (event.target.id === 'typing-input') { event.preventDefault(); $('run-message').textContent = 'Pasting is off during practice so your results reflect your typing.'; }
  });
  document.addEventListener('beforeinput', (event) => {
    if (event.target.id === 'typing-input' && ['insertFromPaste', 'insertFromDrop'].includes(event.inputType)) event.preventDefault();
  });
  for (const name of ['click', 'keyup', 'select']) document.addEventListener(name, (event) => {
    if (event.target.id === 'typing-input' && ui.screen === 'session') paintTarget(currentSnapshot());
  });
  window.addEventListener('blur', () => {
    if (ui.screen === 'session' && ui.run && ui.run.started && !ui.run.paused && !ui.run.finished) {
      ui.run.pause(performance.now()); updateLive(currentSnapshot(), true);
      if ($('run-message')) $('run-message').textContent = 'Paused while the window was away. Press Resume when you return.';
    }
  });
  $('dialog-cancel').addEventListener('click', () => closeDialog(false));
  $('dialog-confirm').addEventListener('click', () => closeDialog(true));
  $('confirm-dialog').addEventListener('cancel', (event) => { event.preventDefault(); closeDialog(false); });

  ui.profile = loadProfile();
  render();
})();
