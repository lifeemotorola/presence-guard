/* Forma Studio model tests. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
import test from 'node:test';
import assert from 'node:assert/strict';
import { createStarterProject, createObject, validateProject, evaluatedTransform, putKeyframe, arrayOffsets, transform, clone, History, MAX_OBJECTS } from '../app/model.js';

test('starter project round-trips without losing scene, materials, or camera', () => {
  const starter = createStarterProject();
  assert.deepEqual(validateProject(JSON.parse(JSON.stringify(starter))), starter);
  assert.equal(starter.objects.length, 4);
  assert.deepEqual(starter.camera.target, [0, 0.9, 0]);
});

test('invalid or future projects fail before replacing any scene', () => {
  const valid = createStarterProject();
  const cases = [
    { ...valid, version: 2 }, { ...valid, format: 'other' }, { ...valid, objects: new Array(MAX_OBJECTS + 1).fill(valid.objects[0]) },
    { ...valid, timeline: { fps: 24, end: 0 } },
    { ...valid, objects: [valid.objects[0], valid.objects[0]] },
    { ...valid, objects: [{ ...valid.objects[0], type: 'javascript' }] },
    { ...valid, objects: [{ ...valid.objects[0], visible: 'yes' }] },
    { ...valid, objects: [{ ...valid.objects[0], transform: { ...valid.objects[0].transform, position: [NaN, 0, 0] } }] },
    { ...valid, objects: [{ ...valid.objects[0], transform: { ...valid.objects[0].transform, scale: [0, 1, 1] } }] },
    { ...valid, objects: [{ ...valid.objects[0], array: { ...valid.objects[0].array, count: 21 } }] },
    { ...valid, objects: [{ ...valid.objects[0], material: { ...valid.objects[0].material, color: 'url(http://x)' } }] },
  ];
  for (const value of cases) assert.throws(() => validateProject(value), /Invalid Forma project/);
  assert.equal(valid.objects.length, 4);
});

test('keyframes sort, replace, interpolate, and round-trip', () => {
  const object = createObject('cube', 'cube-1');
  putKeyframe(object, 24, transform([24, 0, 0], [0, 90, 0], [2, 2, 2]));
  putKeyframe(object, 0, transform([0, 0, 0], [0, 0, 0], [1, 1, 1]));
  assert.deepEqual(object.keyframes.map((key) => key.frame), [0, 24]);
  assert.deepEqual(evaluatedTransform(object, 12), transform([12, 0, 0], [0, 45, 0], [1.5, 1.5, 1.5]));
  assert.deepEqual(evaluatedTransform(object, 200), object.keyframes[1].transform);
  putKeyframe(object, 24, transform([48, 0, 0]));
  assert.equal(object.keyframes.length, 2);
  assert.equal(evaluatedTransform(object, 12).position[0], 24);
  const project = createStarterProject(); project.objects.push(object);
  assert.deepEqual(validateProject(project).objects.at(-1).keyframes, object.keyframes);
});

test('invalid and out-of-range keyframes are rejected', () => {
  const project = createStarterProject();
  putKeyframe(project.objects[0], 121, transform());
  assert.throws(() => validateProject(project), /keyframe/);
  project.objects[0].keyframes = [{ frame: 1, transform: transform() }, { frame: 1, transform: transform() }];
  assert.throws(() => validateProject(project), /duplicate keyframe/);
});

test('procedural arrays are centered, bounded, and genuinely radial', () => {
  assert.deepEqual(arrayOffsets({ enabled: false, count: 5 }), [[0, 0, 0]]);
  assert.deepEqual(arrayOffsets({ enabled: true, mode: 'linear', count: 3, spacing: 2 }), [[-2, 0, 0], [0, 0, 0], [2, 0, 0]]);
  const offsets = arrayOffsets({ enabled: true, mode: 'radial', count: 4, radius: 3 });
  assert.equal(offsets.length, 4);
  for (const [x, y, z] of offsets) { assert.equal(y, 0); assert.ok(Math.abs(Math.hypot(x, z) - 3) < 1e-10); }
});

test('history snapshots are independent, bounded, and redo is cleared by a new edit', () => {
  const history = new History(2), first = createStarterProject(), second = clone(first);
  second.name = 'Second'; history.push(first); history.push(second);
  second.name = 'Mutated later';
  const previous = history.undo({ name: 'Current' });
  assert.equal(previous.name, 'Second');
  assert.equal(history.redo({ name: 'At second' }).name, 'Current');
  history.push({ name: 'Third' }); history.push({ name: 'Fourth' });
  assert.equal(history.past.length, 2);
  assert.equal(history.future.length, 0);
});
