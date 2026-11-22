/* Forma Studio editor. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
import { createStarterProject, createObject, validateProject, evaluatedTransform, putKeyframe, clone, History, TYPE_LABELS, MAX_OBJECTS } from './model.js';
import { Viewport } from './viewport.js';

const $ = (selector) => document.querySelector(selector);
const $$ = (selector) => [...document.querySelectorAll(selector)];
const clamp = (value, min, max) => Math.max(min, Math.min(max, value));
const format = (value) => Number(value.toFixed(2)).toString();
const icon = (name) => `<svg aria-hidden="true"><use href="#i-${name}"></use></svg>`;
const glyph = (type) => ({ 'rounded-box': 'cube', cube: 'cube', sphere: 'sphere', cylinder: 'cylinder', cone: 'cone', torus: 'torus', plane: 'plane', icosahedron: 'sphere' }[type]);
const state = {
  project: createStarterProject(), selectedId: 'starter-form', frame: 0,
  tab: 'object', tool: 'translate', playing: false, playStart: 0, playFrame: 0,
  history: new History(), saved: '', nextId: 0, dragBefore: null, viewport: null,
};
state.saved = JSON.stringify(state.project);
let statusTimer;
let saveInProgress = false;

function say(message) {
  $('#status-text').textContent = message;
  clearTimeout(statusTimer);
  statusTimer = setTimeout(() => { $('#status-text').textContent = 'Ready to create'; }, 7000);
}
function selected() { return state.project.objects.find((object) => object.id === state.selectedId) || null; }
function snapshot() { return { project: clone(state.project), selectedId: state.selectedId, frame: state.frame }; }
function isDirty() { return JSON.stringify(state.project) !== state.saved; }
function refreshDirty() {
  $('#dirty-indicator').hidden = !isDirty();
  $('#project-title').textContent = state.project.name;
  document.title = `${isDirty() ? '● ' : ''}${state.project.name} — Forma Studio`;
  $('#undo').disabled = state.history.past.length === 0;
  $('#redo').disabled = state.history.future.length === 0;
}
function stopPlayback() {
  if (!state.playing) return;
  state.playing = false;
  $('#play').innerHTML = icon('play');
  $('#play').setAttribute('aria-label', 'Play animation');
  $('#play').title = 'Play animation';
}
function restore(value) {
  state.project = validateProject(value.project);
  state.selectedId = state.project.objects.some((object) => object.id === value.selectedId) ? value.selectedId : null;
  state.frame = clamp(value.frame, 0, state.project.timeline.end);
  stopPlayback();
  state.viewport?.setCamera(state.project.camera);
  redraw();
}
function change(action) {
  stopPlayback();
  const before = snapshot();
  try {
    action();
    state.project = validateProject(state.project);
    if (JSON.stringify(before.project) !== JSON.stringify(state.project)) state.history.push(before);
    redraw();
    return true;
  } catch (error) {
    state.project = before.project;
    state.selectedId = before.selectedId;
    state.frame = before.frame;
    redraw();
    say(error.message || 'Could not make that change.');
    return false;
  }
}
function undo() {
  const previous = state.history.undo(snapshot());
  if (previous) { restore(previous); say('Undid the last change.'); }
}
function redo() {
  const next = state.history.redo(snapshot());
  if (next) { restore(next); say('Redid the last change.'); }
}
function selectObject(id) {
  stopPlayback();
  state.selectedId = state.project.objects.some((object) => object.id === id) ? id : null;
  state.viewport?.select(state.selectedId);
  renderObjectList();
  renderInspector();
  renderTimeline();
}
function addShape(type) {
  if (state.project.objects.length >= MAX_OBJECTS) { say(`A scene can contain up to ${MAX_OBJECTS} objects.`); return; }
  const count = state.project.objects.filter((object) => object.type === type).length + 1;
  const id = `object-${Date.now().toString(36)}-${(++state.nextId).toString(36)}`;
  change(() => {
    const object = createObject(type, id, `${TYPE_LABELS[type]} ${String(count).padStart(2, '0')}`);
    state.project.objects.push(object);
    state.selectedId = id;
  });
  say(`${TYPE_LABELS[type]} added. Use the move tool or inspector to position it.`);
}
function duplicateObject() {
  const source = selected();
  if (!source) return;
  if (state.project.objects.length >= MAX_OBJECTS) { say(`A scene can contain up to ${MAX_OBJECTS} objects.`); return; }
  const id = `object-${Date.now().toString(36)}-${(++state.nextId).toString(36)}`;
  change(() => {
    const copy = clone(source);
    copy.id = id;
    copy.name = `${source.name.slice(0, 73)} copy`;
    copy.transform.position[0] = clamp(copy.transform.position[0] + 1, -1000, 1000);
    for (const key of copy.keyframes) key.transform.position[0] = clamp(key.transform.position[0] + 1, -1000, 1000);
    state.project.objects.push(copy);
    state.selectedId = id;
  });
  say('Duplicated the selected object.');
}
function deleteObject() {
  const object = selected();
  if (!object) return;
  change(() => {
    state.project.objects = state.project.objects.filter((item) => item.id !== object.id);
    state.selectedId = null;
  });
  say('Object deleted. Use Undo to bring it back.');
}
function updateTransform(object, value) {
  if (object.keyframes.length) putKeyframe(object, state.frame, value);
  else object.transform = clone(value);
}
function redraw() {
  state.viewport?.rebuild(state.project);
  state.viewport?.setFrame(state.frame);
  state.viewport?.select(state.selectedId);
  renderObjectList();
  renderInspector();
  renderTimeline();
  refreshDirty();
}
function renderObjectList() {
  const list = $('#object-list');
  list.replaceChildren();
  $('#object-count').textContent = String(state.project.objects.length);
  $('#scene-stats').textContent = `${state.project.objects.length} ${state.project.objects.length === 1 ? 'object' : 'objects'}`;
  if (!state.project.objects.length) {
    const empty = document.createElement('div');
    empty.className = 'object-row empty';
    empty.textContent = 'Your scene is empty. Add a shape below to get started.';
    list.append(empty);
    return;
  }
  for (const object of [...state.project.objects].reverse()) {
    const row = document.createElement('div');
    row.className = `object-row${object.id === state.selectedId ? ' selected' : ''}`;
    row.setAttribute('role', 'listitem');
    const button = document.createElement('button');
    button.type = 'button'; button.className = 'object-select'; button.dataset.select = object.id;
    button.setAttribute('aria-label', `Select ${object.name}`);
    button.setAttribute('aria-pressed', String(object.id === state.selectedId));
    const glyphSpan = document.createElement('span'); glyphSpan.className = 'object-glyph'; glyphSpan.innerHTML = icon(glyph(object.type));
    const label = document.createElement('span'); label.className = 'object-label'; label.textContent = object.name;
    button.append(glyphSpan, label);
    const visibility = document.createElement('button');
    visibility.type = 'button'; visibility.className = 'icon-button visibility-button'; visibility.dataset.visibility = object.id;
    visibility.setAttribute('aria-label', `${object.visible ? 'Hide' : 'Show'} ${object.name}`);
    visibility.title = object.visible ? 'Hide object' : 'Show object';
    visibility.innerHTML = icon(object.visible ? 'eye' : 'eye-off');
    row.append(button, visibility); list.append(row);
  }
}
function buildTransformFields() {
  const host = $('#transform-fields');
  host.replaceChildren();
  for (const [property, label, suffix] of [['position', 'Position', 'UNITS'], ['rotation', 'Rotation', 'DEGREES'], ['scale', 'Scale', '×']]) {
    const row = document.createElement('div'); row.className = 'transform-row';
    const title = document.createElement('div'); title.className = 'transform-row-head';
    const name = document.createElement('span'); name.textContent = label;
    const unit = document.createElement('span'); unit.textContent = suffix;
    title.append(name, unit);
    const fields = document.createElement('div'); fields.className = 'axis-fields';
    for (const [index, axis] of ['X', 'Y', 'Z'].entries()) {
      const field = document.createElement('label'); field.className = 'axis-field';
      const letter = document.createElement('span'); letter.textContent = axis;
      const input = document.createElement('input'); input.type = 'number'; input.step = property === 'rotation' ? '1' : '0.1';
      input.dataset.property = property; input.dataset.axis = String(index); input.setAttribute('aria-label', `${label} ${axis}`);
      if (property === 'scale') { input.min = '0.01'; input.max = '100'; }
      else if (property === 'rotation') { input.min = '-36000'; input.max = '36000'; }
      else { input.min = '-1000'; input.max = '1000'; }
      field.append(letter, input); fields.append(field);
    }
    row.append(title, fields); host.append(row);
  }
}
function updateTransformInputs() {
  const object = selected();
  if (!object) return;
  const current = evaluatedTransform(object, state.frame);
  for (const input of $$('#transform-fields input')) input.value = format(current[input.dataset.property][Number(input.dataset.axis)]);
}
function renderInspector() {
  for (const tab of $$('.inspector-tabs [role=tab]')) tab.setAttribute('aria-selected', String(tab.dataset.tab === state.tab));
  const object = selected();
  $('#no-selection').hidden = Boolean(object) || state.tab === 'scene';
  $('#pane-object').hidden = state.tab !== 'object' || !object;
  $('#pane-material').hidden = state.tab !== 'material' || !object;
  $('#pane-scene').hidden = state.tab !== 'scene';
  $('#project-name-input').value = state.project.name;
  $('#background-color').value = state.project.world.background;
  $('#background-value').textContent = state.project.world.background.toUpperCase();
  for (const [id, key] of [['ambient', 'ambient'], ['key-light', 'key'], ['exposure', 'exposure']]) {
    $(`#${id}`).value = state.project.world[key]; $(`#${id}-value`).textContent = state.project.world[key].toFixed(2);
  }
  if (!object) return;
  $('#selected-name').textContent = object.name;
  $('#object-name').value = object.name;
  updateTransformInputs();
  $('#array-enabled').checked = object.array.enabled;
  $('#array-controls').hidden = !object.array.enabled;
  $('#array-mode').value = object.array.mode;
  $('#array-count').value = object.array.count;
  $('#array-distance').value = object.array.mode === 'radial' ? object.array.radius : object.array.spacing;
  $('#array-distance-label').firstChild.textContent = object.array.mode === 'radial' ? 'Radius' : 'Spacing';
  $('#material-color').value = object.material.color;
  $('#color-value').textContent = object.material.color.toUpperCase();
  $('#roughness').value = object.material.roughness; $('#roughness-value').textContent = object.material.roughness.toFixed(2);
  $('#metalness').value = object.material.metalness; $('#metalness-value').textContent = object.material.metalness.toFixed(2);
}
function renderTimeline() {
  const object = selected();
  const end = state.project.timeline.end;
  $('#fps-display').textContent = `${state.project.timeline.fps} FPS`;
  $('#end-frame').value = end;
  $('#end-readout').textContent = `/ ${end}`;
  $('#ruler-end').textContent = String(end);
  $('#frame-input').max = end; $('#frame-input').value = state.frame;
  $('#frame-range').max = end; $('#frame-range').value = state.frame;
  $('#track-name').textContent = object?.name || 'Select an object';
  $('#add-key').disabled = !object;
  $('#delete-key').disabled = !object || !object.keyframes.some((key) => key.frame === state.frame);
  const markers = $('#key-markers'); markers.replaceChildren();
  if (object) for (const key of object.keyframes) {
    const button = document.createElement('button');
    button.type = 'button'; button.className = `key-marker${key.frame === state.frame ? ' current' : ''}`;
    button.dataset.frame = String(key.frame); button.style.left = `${key.frame / end * 100}%`;
    button.title = `Go to keyframe ${key.frame}`; button.setAttribute('aria-label', `Go to keyframe ${key.frame}`);
    markers.append(button);
  }
}
function setFrame(value) {
  state.frame = clamp(Math.round(Number.isFinite(value) ? value : 0), 0, state.project.timeline.end);
  state.viewport?.setFrame(state.frame);
  renderTimeline();
  updateTransformInputs();
}
function addKey() {
  const object = selected(); if (!object) return;
  const value = evaluatedTransform(object, state.frame);
  if (change(() => putKeyframe(object, state.frame, value))) say(`Keyframe saved at frame ${state.frame}. Transform edits now auto-key this object.`);
}
function removeKey() {
  const object = selected(); if (!object) return;
  const value = evaluatedTransform(object, state.frame);
  if (!object.keyframes.some((key) => key.frame === state.frame)) return;
  change(() => {
    object.keyframes = object.keyframes.filter((key) => key.frame !== state.frame);
    if (!object.keyframes.length) object.transform = value;
  });
  say('Keyframe removed.');
}
function togglePlayback() {
  if (state.playing) { stopPlayback(); return; }
  state.playing = true; state.playStart = 0; state.playFrame = state.frame;
  $('#play').innerHTML = icon('pause'); $('#play').setAttribute('aria-label', 'Pause animation'); $('#play').title = 'Pause animation';
}
function onTick(time) {
  if (!state.playing) return;
  if (!state.playStart) state.playStart = time;
  const elapsed = Math.floor((time - state.playStart) * state.project.timeline.fps / 1000);
  const next = (state.playFrame + elapsed) % (state.project.timeline.end + 1);
  if (next !== state.frame) setFrame(next);
}
function setTool(tool) {
  state.tool = tool;
  for (const button of $$('[data-tool]')) { const active = button.dataset.tool === tool; button.classList.toggle('active', active); button.setAttribute('aria-pressed', String(active)); }
  state.viewport?.setTool(tool);
}
function safeName() {
  return state.project.name.replace(/[\\/:*?"<>|\u0000-\u001f]/g, '-').trim().replace(/[. ]+$/g, '').slice(0, 70) || 'Untitled scene';
}
function download(bytes, filename, type) {
  const url = URL.createObjectURL(new Blob([bytes], { type }));
  const link = document.createElement('a'); link.href = url; link.download = filename; link.hidden = true;
  document.body.append(link); link.click(); link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 60000);
}
async function saveProject(saveAs = false) {
  if (saveInProgress) { say('A save is already in progress.'); return false; }
  saveInProgress = true;
  try {
    if (state.viewport) state.project.camera = state.viewport.getCamera();
    const projectToSave = validateProject(state.project);
    const savedSnapshot = JSON.stringify(projectToSave);
    const text = JSON.stringify(projectToSave, null, 2);
    if (window.forma) {
      const result = await window.forma.saveProject(text, safeName(), saveAs);
      if (result.canceled) return false;
    } else download(text, `${safeName()}.forma`, 'application/json');
    // The native dialog and disk write are asynchronous. Do not mark edits
    // made while the save was pending as saved or discard them on New/Open.
    state.saved = savedSnapshot;
    refreshDirty();
    if (isDirty()) { say('Project saved, but newer changes are not saved yet.'); return false; }
    say('Project saved.'); return true;
  } catch (error) { say(`Could not save: ${error.message}`); return false; }
  finally { saveInProgress = false; refreshDirty(); }
}
function confirmUnsaved() {
  if (!isDirty()) return Promise.resolve(true);
  const dialog = $('#confirm-dialog');
  return new Promise((resolve) => {
    const finish = async (choice) => {
      dialog.removeEventListener('click', handler); dialog.removeEventListener('cancel', cancel);
      if (dialog.open) dialog.close();
      if (choice === 'save') resolve(await saveProject());
      else resolve(choice === 'discard');
    };
    const handler = (event) => { const button = event.target.closest('[data-confirm]'); if (button) finish(button.dataset.confirm); };
    const cancel = (event) => { event.preventDefault(); finish('cancel'); };
    dialog.addEventListener('click', handler); dialog.addEventListener('cancel', cancel); dialog.showModal();
    dialog.querySelector('[data-confirm=cancel]').focus();
  });
}
async function newProject() {
  if (!await confirmUnsaved()) return;
  if (window.forma) await window.forma.forgetProject();
  const project = createStarterProject(); project.objects = [];
  state.project = project; state.selectedId = null; state.frame = 0; state.history.clear();
  state.saved = JSON.stringify(project); state.viewport?.setCamera(project.camera); redraw(); say('New scene ready. Add a shape to begin.');
}
function parseProject(text) {
  if (typeof text !== 'string' || text.length > 10 * 1024 * 1024) throw new Error('Project file is too large.');
  return validateProject(JSON.parse(text));
}
async function loadText(text, acceptNative = false) {
  try {
    const project = parseProject(text);
    if (acceptNative) await window.forma.acceptOpenedProject();
    state.project = project; state.selectedId = project.objects[0]?.id || null; state.frame = 0;
    state.history.clear(); state.saved = JSON.stringify(project);
    state.viewport?.setCamera(project.camera); redraw(); say('Project opened.');
  } catch (error) { say(`Could not open project: ${error.message}`); }
}
async function openProject() {
  if (!await confirmUnsaved()) return;
  if (window.forma) {
    try { const result = await window.forma.openProject(); if (!result.canceled) await loadText(result.text, true); }
    catch (error) { say(`Could not open project: ${error.message}`); }
  } else { $('#file-input').value = ''; $('#file-input').click(); }
}
async function exportFile(kind) {
  if (!state.viewport) { say('The 3D view must be available to export.'); return; }
  stopPlayback();
  try {
    say(kind === 'glb' ? 'Preparing your 3D model…' : 'Preparing your viewport image…');
    const data = kind === 'glb' ? await state.viewport.exportGlb() : await state.viewport.renderPng();
    if (!(data instanceof ArrayBuffer)) throw new Error('The exporter did not return a file.');
    if (window.forma) {
      const result = await window.forma.exportFile(kind, new Uint8Array(data), safeName());
      if (result.canceled) { say('Export canceled.'); return; }
    } else download(data, `${safeName()}.${kind}`, kind === 'glb' ? 'model/gltf-binary' : 'image/png');
    say(kind === 'glb' ? 'Exported the current frame as GLB. Animation is not included.' : 'Saved the realtime viewport as a PNG image.');
  } catch (error) { say(`Could not export: ${error.message}`); }
}

buildTransformFields();
try {
  state.viewport = new Viewport($('#viewport-canvas'), state.project, {
    onSelect: selectObject,
    onTick,
    onCamera: (camera) => { state.project.camera = camera; refreshDirty(); },
    onDragStart: () => { stopPlayback(); state.dragBefore = snapshot(); },
    onDrag: (value) => {
      const object = selected(); if (!object) return;
      try { updateTransform(object, value); updateTransformInputs(); renderTimeline(); refreshDirty(); }
      catch (error) { say(error.message); }
    },
    onDragEnd: () => {
      if (state.dragBefore && JSON.stringify(state.dragBefore.project) !== JSON.stringify(state.project)) state.history.push(state.dragBefore);
      state.dragBefore = null; refreshDirty();
      // Reconcile the gizmo with the model after mouseUp; an edit may have
      // been refused (for example, at the keyframe limit).
      requestAnimationFrame(() => { if (!state.dragBefore) redraw(); });
    },
    onError: say,
  });
  state.viewport.select(state.selectedId);
} catch (error) {
  $('#webgl-error').hidden = false;
  say('The 3D view could not start. A supported WebGL graphics driver is required.');
  console.error('WebGL initialization failed:', error);
}
redraw();

$('#object-list').addEventListener('click', (event) => {
  const select = event.target.closest('[data-select]');
  const toggle = event.target.closest('[data-visibility]');
  if (select) selectObject(select.dataset.select);
  if (toggle) change(() => { const object = state.project.objects.find((item) => item.id === toggle.dataset.visibility); object.visible = !object.visible; });
});
for (const button of $$('[data-add]')) button.addEventListener('click', () => addShape(button.dataset.add));
for (const button of $$('[data-tool]')) button.addEventListener('click', () => setTool(button.dataset.tool));
for (const button of $$('.inspector-tabs [role=tab]')) button.addEventListener('click', () => { state.tab = button.dataset.tab; renderInspector(); });
for (const button of $$('[data-layout]')) button.addEventListener('click', () => {
  for (const tab of $$('[data-layout]')) { const active = tab === button; tab.classList.toggle('active', active); tab.setAttribute('aria-pressed', String(active)); }
  $('.center-panel').classList.toggle('animate-mode', button.dataset.layout === 'animate');
});
$('#undo').addEventListener('click', undo); $('#redo').addEventListener('click', redo);
$('#duplicate-object').addEventListener('click', duplicateObject); $('#delete-object').addEventListener('click', deleteObject);
$('#object-name').addEventListener('change', (event) => { const value = event.target.value.trim().slice(0, 80); if (!value) { renderInspector(); say('An object needs a name.'); return; } change(() => { selected().name = value; }); });
$('#transform-fields').addEventListener('change', (event) => {
  const input = event.target.closest('input[data-property]'); const object = selected(); if (!input || !object) return;
  const property = input.dataset.property, axis = Number(input.dataset.axis);
  const raw = input.valueAsNumber;
  if (!Number.isFinite(raw)) { updateTransformInputs(); return; }
  const value = evaluatedTransform(object, state.frame);
  value[property][axis] = property === 'scale' ? clamp(raw, 0.01, 100) : property === 'rotation' ? clamp(raw, -36000, 36000) : clamp(raw, -1000, 1000);
  if (change(() => updateTransform(object, value)) && object.keyframes.length) say(`Auto-keyed transform at frame ${state.frame}.`);
});
$('#array-enabled').addEventListener('change', (event) => change(() => { selected().array.enabled = event.target.checked; }));
$('#array-mode').addEventListener('change', (event) => change(() => { selected().array.mode = event.target.value; }));
$('#array-count').addEventListener('change', (event) => { const value = event.target.valueAsNumber; if (!Number.isFinite(value)) { renderInspector(); return; } change(() => { selected().array.count = clamp(Math.round(value), 1, 20); }); });
$('#array-distance').addEventListener('change', (event) => { const value = event.target.valueAsNumber; if (!Number.isFinite(value)) { renderInspector(); return; } change(() => { const array = selected().array; array[array.mode === 'radial' ? 'radius' : 'spacing'] = clamp(value, 0.1, 20); }); });
$('#material-color').addEventListener('change', (event) => change(() => { selected().material.color = event.target.value; }));
for (const key of ['roughness', 'metalness']) {
  $(`#${key}`).addEventListener('input', (event) => { $(`#${key}-value`).textContent = Number(event.target.value).toFixed(2); });
  $(`#${key}`).addEventListener('change', (event) => change(() => { selected().material[key] = clamp(Number(event.target.value), 0, 1); }));
}
$('#project-name-input').addEventListener('change', (event) => { const value = event.target.value.trim().slice(0, 100); if (!value) { renderInspector(); say('A project needs a name.'); return; } change(() => { state.project.name = value; }); });
$('#background-color').addEventListener('change', (event) => change(() => { state.project.world.background = event.target.value; }));
for (const [id, key, min, max] of [['ambient', 'ambient', 0, 3], ['key-light', 'key', 0, 6], ['exposure', 'exposure', 0.1, 3]]) {
  $(`#${id}`).addEventListener('input', (event) => { $(`#${id}-value`).textContent = Number(event.target.value).toFixed(2); });
  $(`#${id}`).addEventListener('change', (event) => change(() => { state.project.world[key] = clamp(Number(event.target.value), min, max); }));
}
$('#focus-object').addEventListener('click', () => { if (!state.viewport?.frameSelected()) say('Select a visible object to frame it.'); });
$('#view-select').addEventListener('change', (event) => state.viewport?.setView(event.target.value));
$('#toggle-grid').addEventListener('click', (event) => { const button = event.currentTarget; const value = button.getAttribute('aria-pressed') !== 'true'; button.setAttribute('aria-pressed', String(value)); button.classList.toggle('active', value); state.viewport?.setGrid(value); });
$('#wireframe').addEventListener('click', (event) => { const button = event.currentTarget; const value = button.getAttribute('aria-pressed') !== 'true'; button.setAttribute('aria-pressed', String(value)); button.classList.toggle('active', value); state.viewport?.setWireframe(value); });
$('#frame-range').addEventListener('input', (event) => { stopPlayback(); setFrame(Number(event.target.value)); });
$('#frame-input').addEventListener('change', (event) => { stopPlayback(); setFrame(event.target.valueAsNumber); });
$('#end-frame').addEventListener('change', (event) => {
  const raw = event.target.valueAsNumber; if (!Number.isFinite(raw)) { renderTimeline(); return; }
  const value = clamp(Math.round(raw), 1, 600);
  const latest = Math.max(0, ...state.project.objects.flatMap((object) => object.keyframes.map((key) => key.frame)));
  if (value < latest) { renderTimeline(); say(`Delete keyframes after frame ${value} before shortening the timeline.`); return; }
  change(() => { state.project.timeline.end = value; state.frame = Math.min(state.frame, value); });
});
$('#key-markers').addEventListener('click', (event) => { const button = event.target.closest('[data-frame]'); if (button) { stopPlayback(); setFrame(Number(button.dataset.frame)); } });
$('#play').addEventListener('click', togglePlayback); $('#first-frame').addEventListener('click', () => { stopPlayback(); setFrame(0); }); $('#last-frame').addEventListener('click', () => { stopPlayback(); setFrame(state.project.timeline.end); });
$('#add-key').addEventListener('click', addKey); $('#delete-key').addEventListener('click', removeKey);
$('#new-project').addEventListener('click', newProject); $('#open-project').addEventListener('click', openProject);
$('#save-project').addEventListener('click', () => saveProject()); $('#save-as').addEventListener('click', () => saveProject(true));
for (const button of $$('[data-export]')) button.addEventListener('click', () => exportFile(button.dataset.export));
for (const button of $$('.menu-popover button')) button.addEventListener('click', () => { button.closest('details').open = false; });
document.addEventListener('click', (event) => { for (const menu of $$('details.menu[open]')) if (!menu.contains(event.target)) menu.open = false; });
$('#file-input').addEventListener('change', async (event) => { const file = event.target.files?.[0]; if (!file) return; if (file.size > 10 * 1024 * 1024) { say('Project file is too large.'); return; } await loadText(await file.text()); });
$('#help').addEventListener('click', () => $('#help-dialog').showModal()); $('#close-help').addEventListener('click', () => $('#help-dialog').close());
document.addEventListener('keydown', (event) => {
  if (document.querySelector('dialog[open]')) return;
  const key = event.key.toLowerCase(), mod = event.ctrlKey || event.metaKey;
  const editing = event.target.matches('input, select, textarea') || event.target.isContentEditable;
  if (mod && ['s', 'o', 'n'].includes(key)) {
    event.preventDefault();
    // Commit a focused property before saving or checking for unsaved work.
    if (editing) event.target.blur();
    if (key === 's') saveProject(event.shiftKey);
    else if (key === 'o') openProject();
    else newProject();
    return;
  }
  // Keep ordinary text-editing shortcuts and letters inside form fields.
  if (editing) return;
  if (mod && key === 'z') { event.preventDefault(); event.shiftKey ? redo() : undo(); }
  else if (mod && key === 'y') { event.preventDefault(); redo(); }
  else if (mod && key === 'd') { event.preventDefault(); duplicateObject(); }
  else if (!mod && key === 'delete') { event.preventDefault(); deleteObject(); }
  else if (!mod && key === 'q') setTool('select');
  else if (!mod && key === 'g') setTool('translate');
  else if (!mod && key === 'r') setTool('rotate');
  else if (!mod && key === 's') setTool('scale');
  else if (!mod && key === 'f') state.viewport?.frameSelected();
  else if (!mod && key === ' ') { event.preventDefault(); togglePlayback(); }
});
window.addEventListener('beforeunload', (event) => { if (isDirty()) { event.preventDefault(); event.returnValue = ''; } });
