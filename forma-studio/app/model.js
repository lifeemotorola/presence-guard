/* Forma Studio scene model. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
import { validateMesh, MAX_PROJECT_MESH_VERTICES, MAX_PROJECT_MESH_FACES } from './mesh.js';

export const FORMAT = 'forma-studio';
export const VERSION = 2;
export const MAX_OBJECTS = 120;
export const MAX_KEYS = 300;
export const PRIMITIVE_TYPES = ['rounded-box', 'cube', 'sphere', 'cylinder', 'cone', 'torus', 'plane', 'icosahedron'];
export const TYPES = [...PRIMITIVE_TYPES, 'mesh'];
export const TYPE_LABELS = {
  'rounded-box': 'Rounded cube', cube: 'Cube', sphere: 'Sphere', cylinder: 'Cylinder',
  cone: 'Cone', torus: 'Torus', plane: 'Plane', icosahedron: 'Icosphere', mesh: 'Editable mesh',
};

export const clone = (value) => JSON.parse(JSON.stringify(value));
export function transform(position = [0, 0.8, 0], rotation = [0, 0, 0], scale = [1, 1, 1]) {
  return { position: [...position], rotation: [...rotation], scale: [...scale] };
}
export function createObject(type, id, name = TYPE_LABELS[type]) {
  if (!PRIMITIVE_TYPES.includes(type)) throw new Error('Unknown shape');
  return {
    id, name, type, visible: true,
    transform: transform(type === 'plane' ? [0, 0.01, 0] : [0, 0.8, 0]),
    material: { color: '#a991f7', roughness: 0.4, metalness: 0.08 },
    array: { enabled: false, mode: 'linear', count: 5, spacing: 1.8, radius: 3 },
    keyframes: [],
  };
}
export function createStarterProject() {
  const main = createObject('rounded-box', 'starter-form', 'Form 01');
  main.transform = transform([-0.45, 1.15, 0], [-8, 27, -7], [1.35, 1.35, 1.35]);
  main.material = { color: '#a78bfa', roughness: 0.34, metalness: 0.08 };
  const ring = createObject('torus', 'starter-ring', 'Orbit ring');
  ring.transform = transform([-0.45, 1.15, 0], [69, -14, -24], [1.8, 1.8, 1.8]);
  ring.material = { color: '#f2ab96', roughness: 0.31, metalness: 0.24 };
  const orb = createObject('sphere', 'starter-orb', 'Mint sphere');
  orb.transform = transform([1.55, 0.65, 0.45], [0, 0, 0], [0.75, 0.75, 0.75]);
  orb.material = { color: '#91d8c9', roughness: 0.3, metalness: 0.05 };
  const stage = createObject('cylinder', 'starter-stage', 'Stage');
  stage.transform = transform([0, -0.02, 0], [0, 0, 0], [2.55, 0.16, 2.55]);
  stage.material = { color: '#6b6481', roughness: 0.6, metalness: 0.08 };
  return {
    format: FORMAT, version: VERSION, name: 'Untitled scene',
    objects: [stage, ring, main, orb],
    world: { background: '#252632', ambient: 1.1, key: 2.7, exposure: 1 },
    timeline: { fps: 24, end: 120 },
    camera: { position: [6.4, 4.7, 7.4], target: [0, 0.9, 0] },
  };
}

const fail = (message) => { throw new Error(`Invalid Forma project: ${message}`); };
function record(value, label) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) fail(label);
  return value;
}
function number(value, label, min, max, integer = false) {
  if (typeof value !== 'number' || !Number.isFinite(value) || value < min || value > max || (integer && !Number.isInteger(value))) fail(label);
  return value;
}
function text(value, label, max) {
  if (typeof value !== 'string' || !value.trim() || value.length > max || /[\u0000-\u001f]/.test(value)) fail(label);
  return value;
}
function bool(value, label) { if (typeof value !== 'boolean') fail(label); return value; }
function vector(value, label, min, max) {
  if (!Array.isArray(value) || value.length !== 3) fail(label);
  return value.map((item) => number(item, label, min, max));
}
function checkedTransform(value, label) {
  record(value, label);
  return {
    position: vector(value.position, `${label} position`, -1000, 1000),
    rotation: vector(value.rotation, `${label} rotation`, -36000, 36000),
    scale: vector(value.scale, `${label} scale`, 0.01, 100),
  };
}
function color(value, label) {
  if (typeof value !== 'string' || !/^#[0-9a-f]{6}$/i.test(value)) fail(label);
  return value.toLowerCase();
}

// Parse an untrusted project without changing the current scene. Explicit
// bounds protect the renderer from malformed, huge, or non-finite values.
export function validateProject(value) {
  record(value, 'root');
  // Version 1 projects contain only primitives. Read them into version 2;
  // version 2 files are intentionally rejected by the older 0.1 app.
  if (value.format !== FORMAT || ![1, VERSION].includes(value.version)) fail('unsupported format or version');
  const sourceVersion = value.version;
  const name = text(value.name, 'project name', 100);
  if (!Array.isArray(value.objects) || value.objects.length > MAX_OBJECTS) fail('object count');
  const timeline = record(value.timeline, 'timeline');
  const end = number(timeline.end, 'end frame', 1, 600, true);
  const fps = number(timeline.fps, 'frames per second', 1, 60, true);
  const world = record(value.world, 'world');
  const camera = record(value.camera, 'camera');
  const ids = new Set();
  let totalMeshVertices = 0, totalMeshFaces = 0;
  const objects = value.objects.map((item) => {
    record(item, 'object');
    const id = text(item.id, 'object ID', 80);
    if (!/^[a-z0-9_-]+$/i.test(id) || ids.has(id)) fail('object ID must be unique');
    ids.add(id);
    if (!TYPES.includes(item.type) || (sourceVersion === 1 && item.type === 'mesh')) fail('unknown object type');
    const material = record(item.material, 'material');
    const array = record(item.array, 'array');
    if (!['linear', 'radial'].includes(array.mode)) fail('array mode');
    if (!Array.isArray(item.keyframes) || item.keyframes.length > MAX_KEYS) fail('keyframe count');
    const frames = new Set();
    const keyframes = item.keyframes.map((key) => {
      record(key, 'keyframe');
      const frame = number(key.frame, 'keyframe', 0, end, true);
      if (frames.has(frame)) fail('duplicate keyframe');
      frames.add(frame);
      return { frame, transform: checkedTransform(key.transform, 'keyframe transform') };
    }).sort((a, b) => a.frame - b.frame);
    const object = {
      id, name: text(item.name, 'object name', 80), type: item.type, visible: bool(item.visible, 'visibility'),
      transform: checkedTransform(item.transform, 'object transform'),
      material: { color: color(material.color, 'material color'), roughness: number(material.roughness, 'roughness', 0, 1), metalness: number(material.metalness, 'metalness', 0, 1) },
      array: { enabled: bool(array.enabled, 'array enabled'), mode: array.mode, count: number(array.count, 'array count', 1, 20, true), spacing: number(array.spacing, 'array spacing', 0.1, 20), radius: number(array.radius, 'array radius', 0.1, 20) },
      keyframes,
    };
    if (item.type === 'mesh') {
      try { object.mesh = validateMesh(item.mesh); } catch (error) { fail(error.message); }
      object.shading = item.shading === undefined ? 'flat' : item.shading;
      if (!['flat', 'smooth'].includes(object.shading)) fail('mesh shading');
      totalMeshVertices += object.mesh.vertices.length;
      totalMeshFaces += object.mesh.faces.length;
      if (totalMeshVertices > MAX_PROJECT_MESH_VERTICES || totalMeshFaces > MAX_PROJECT_MESH_FACES) fail('total editable mesh size');
    }
    return object;
  });
  return {
    format: FORMAT, version: VERSION, name, objects,
    world: { background: color(world.background, 'background color'), ambient: number(world.ambient, 'ambient light', 0, 3), key: number(world.key, 'key light', 0, 6), exposure: number(world.exposure, 'exposure', 0.1, 3) },
    timeline: { fps, end },
    camera: { position: vector(camera.position, 'camera position', -1000, 1000), target: vector(camera.target, 'camera target', -1000, 1000) },
  };
}

export function evaluatedTransform(object, frame) {
  const keys = object.keyframes;
  if (!keys.length) return clone(object.transform);
  if (frame <= keys[0].frame) return clone(keys[0].transform);
  if (frame >= keys[keys.length - 1].frame) return clone(keys[keys.length - 1].transform);
  const next = keys.findIndex((key) => key.frame >= frame);
  const a = keys[next - 1], b = keys[next];
  const t = (frame - a.frame) / (b.frame - a.frame);
  const lerp = (key) => a.transform[key].map((value, index) => value + (b.transform[key][index] - value) * t);
  return { position: lerp('position'), rotation: lerp('rotation'), scale: lerp('scale') };
}
export function putKeyframe(object, frame, value) {
  const index = object.keyframes.findIndex((key) => key.frame === frame);
  const key = { frame, transform: clone(value) };
  if (index >= 0) object.keyframes[index] = key;
  else {
    if (object.keyframes.length >= MAX_KEYS) throw new Error('This object has reached the keyframe limit.');
    object.keyframes.push(key);
  }
  object.keyframes.sort((a, b) => a.frame - b.frame);
}
export function arrayOffsets(settings) {
  if (!settings.enabled || settings.count === 1) return [[0, 0, 0]];
  return Array.from({ length: settings.count }, (_, index) => {
    if (settings.mode === 'radial') {
      const angle = index * Math.PI * 2 / settings.count;
      return [Math.cos(angle) * settings.radius, 0, Math.sin(angle) * settings.radius];
    }
    return [(index - (settings.count - 1) / 2) * settings.spacing, 0, 0];
  });
}

export class History {
  constructor(limit = 40) { this.limit = limit; this.past = []; this.future = []; }
  push(snapshot) { this.past.push(clone(snapshot)); if (this.past.length > this.limit) this.past.shift(); this.future = []; }
  undo(current) { if (!this.past.length) return null; this.future.push(clone(current)); return this.past.pop(); }
  redo(current) { if (!this.future.length) return null; this.past.push(clone(current)); return this.future.pop(); }
  clear() { this.past = []; this.future = []; }
}
