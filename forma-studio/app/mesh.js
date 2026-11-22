/* Forma Studio editable mesh operations. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */

// Bounded topology keeps untrusted project files and undo snapshots manageable.
export const MAX_MESH_VERTICES = 4096;
export const MAX_MESH_FACES = 8192;
export const MAX_PROJECT_MESH_VERTICES = 24000;
export const MAX_PROJECT_MESH_FACES = 48000;
export const MAX_FACE_CORNERS = 8;
const MAX_COORDINATE = 1000;
const AREA_EPSILON = 1e-16;

const fail = (message) => { throw new Error(`Invalid mesh: ${message}`); };
const copyMesh = (mesh) => ({ vertices: mesh.vertices.map((vertex) => [...vertex]), faces: mesh.faces.map((face) => [...face]) });

// Newell's method works for triangles and planar polygons regardless of the
// first three corners. The winding determines the outward extrusion direction.
export function faceNormal(mesh, faceIndex) {
  const face = mesh.faces[faceIndex];
  if (!face) throw new Error('Select a face first.');
  let x = 0, y = 0, z = 0;
  for (let i = 0; i < face.length; i++) {
    const a = mesh.vertices[face[i]], b = mesh.vertices[face[(i + 1) % face.length]];
    x += (a[1] - b[1]) * (a[2] + b[2]);
    y += (a[2] - b[2]) * (a[0] + b[0]);
    z += (a[0] - b[0]) * (a[1] + b[1]);
  }
  const length = Math.hypot(x, y, z);
  if (length * length <= AREA_EPSILON) throw new Error('This face has no usable area.');
  return [x / length, y / length, z / length];
}

// Triangulate a simple polygon in the dominant plane of its Newell normal.
// Unlike a triangle fan this also handles concave faces. Non-planar faces are
// interpreted as triangles in that projection. Refuse self-intersections and
// degenerate triangles rather than saving geometry the viewport cannot render.
export function triangulateFace(mesh, faceIndex) {
  const face = mesh.faces[faceIndex];
  const normal = faceNormal(mesh, faceIndex);
  if (face.length === 3) return [[face[0], face[1], face[2]]];
  const major = normal.reduce((best, value, index) => Math.abs(value) > Math.abs(normal[best]) ? index : best, 0);
  const points = face.map((index) => {
    const vertex = mesh.vertices[index];
    return major === 0 ? [vertex[1], vertex[2]] : major === 1 ? [vertex[2], vertex[0]] : [vertex[0], vertex[1]];
  });
  const cross = (a, b, c) => (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
  const extent = Math.max(...points.map((p) => Math.max(Math.abs(p[0] - points[0][0]), Math.abs(p[1] - points[0][1]))));
  const epsilon = Math.max(1e-12, extent * extent * 1e-12);
  let signedArea = 0;
  for (let i = 1; i < points.length - 1; i++) signedArea += cross(points[0], points[i], points[i + 1]);
  const winding = Math.sign(signedArea);
  if (!winding || Math.abs(signedArea) <= epsilon) throw new Error('This face has no usable area.');
  const onSegment = (a, b, p) => Math.abs(cross(a, b, p)) <= epsilon &&
    p[0] >= Math.min(a[0], b[0]) - 1e-9 && p[0] <= Math.max(a[0], b[0]) + 1e-9 &&
    p[1] >= Math.min(a[1], b[1]) - 1e-9 && p[1] <= Math.max(a[1], b[1]) + 1e-9;
  const intersects = (a, b, c, d) => {
    const abC = cross(a, b, c), abD = cross(a, b, d), cdA = cross(c, d, a), cdB = cross(c, d, b);
    if (((abC > epsilon && abD < -epsilon) || (abC < -epsilon && abD > epsilon)) &&
        ((cdA > epsilon && cdB < -epsilon) || (cdA < -epsilon && cdB > epsilon))) return true;
    return onSegment(a, b, c) || onSegment(a, b, d) || onSegment(c, d, a) || onSegment(c, d, b);
  };
  for (let i = 0; i < face.length; i++) for (let j = i + 1; j < face.length; j++) {
    if (j === i + 1 || (i === 0 && j === face.length - 1)) continue;
    if (intersects(points[i], points[(i + 1) % face.length], points[j], points[(j + 1) % face.length])) throw new Error('This face intersects itself.');
  }
  const remaining = points.map((_point, index) => index), triangles = [];
  while (remaining.length > 3) {
    let ear = -1;
    for (let i = 0; i < remaining.length; i++) {
      const previous = remaining[(i + remaining.length - 1) % remaining.length];
      const current = remaining[i], next = remaining[(i + 1) % remaining.length];
      const a = points[previous], b = points[current], c = points[next];
      if (cross(a, b, c) * winding <= epsilon) continue;
      const contains = remaining.some((candidate) => candidate !== previous && candidate !== current && candidate !== next &&
        cross(a, b, points[candidate]) * winding >= -epsilon && cross(b, c, points[candidate]) * winding >= -epsilon && cross(c, a, points[candidate]) * winding >= -epsilon);
      if (!contains) { triangles.push([face[previous], face[current], face[next]]); ear = i; break; }
    }
    if (ear < 0) throw new Error('This face cannot be triangulated.');
    remaining.splice(ear, 1);
  }
  if (cross(points[remaining[0]], points[remaining[1]], points[remaining[2]]) * winding <= epsilon) throw new Error('This face cannot be triangulated.');
  triangles.push(remaining.map((index) => face[index]));
  return triangles;
}

export function validateMesh(value) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) fail('missing geometry');
  if (!Array.isArray(value.vertices) || value.vertices.length < 3 || value.vertices.length > MAX_MESH_VERTICES) fail('vertex count');
  if (!Array.isArray(value.faces) || value.faces.length < 1 || value.faces.length > MAX_MESH_FACES) fail('face count');
  const vertices = value.vertices.map((vertex) => {
    if (!Array.isArray(vertex) || vertex.length !== 3 || vertex.some((coordinate) => typeof coordinate !== 'number' || !Number.isFinite(coordinate) || Math.abs(coordinate) > MAX_COORDINATE)) fail('vertex coordinates');
    return [...vertex];
  });
  const faces = value.faces.map((face) => {
    if (!Array.isArray(face) || face.length < 3 || face.length > MAX_FACE_CORNERS) fail('face corner count');
    if (face.some((index) => !Number.isInteger(index) || index < 0 || index >= vertices.length) || new Set(face).size !== face.length) fail('face vertex indices');
    return [...face];
  });
  const mesh = { vertices, faces };
  for (let i = 0; i < faces.length; i++) {
    try { triangulateFace(mesh, i); } catch { fail('degenerate or self-intersecting face'); }
  }
  return mesh;
}

export function createCubeMesh() {
  const a = 0.7;
  return {
    vertices: [[-a, -a, -a], [a, -a, -a], [a, a, -a], [-a, a, -a], [-a, -a, a], [a, -a, a], [a, a, a], [-a, a, a]],
    // Consistent outward-facing winding, one quad per side.
    faces: [[0, 3, 2, 1], [4, 5, 6, 7], [0, 1, 5, 4], [3, 7, 6, 2], [0, 4, 7, 3], [1, 2, 6, 5]],
  };
}
export function createPlaneMesh() {
  return { vertices: [[-1, 0, -1], [-1, 0, 1], [1, 0, 1], [1, 0, -1]], faces: [[0, 1, 2, 3]] };
}

// Convert a triangulated primitive to a shared-vertex mesh. Coincident UV seam
// and cap positions are welded; zero-area triangles at poles are discarded.
export function meshFromTriangles(sourceVertices, sourceIndices) {
  if (!Array.isArray(sourceVertices) || !Array.isArray(sourceIndices) || sourceIndices.length % 3) throw new Error('Invalid primitive geometry.');
  const vertices = [], faces = [], lookup = new Map();
  const vertexFor = (sourceIndex) => {
    const source = sourceVertices[sourceIndex];
    if (!source || source.length !== 3 || source.some((item) => typeof item !== 'number' || !Number.isFinite(item))) throw new Error('Invalid primitive vertex.');
    const vertex = source.map((coordinate) => { const rounded = Math.round(coordinate * 1e6) / 1e6; return Object.is(rounded, -0) ? 0 : rounded; });
    const key = vertex.join(',');
    if (!lookup.has(key)) { lookup.set(key, vertices.length); vertices.push(vertex); }
    return lookup.get(key);
  };
  for (let i = 0; i < sourceIndices.length; i += 3) {
    const face = sourceIndices.slice(i, i + 3).map(vertexFor);
    if (new Set(face).size !== 3) continue;
    const temporary = { vertices, faces: [face] };
    try { faceNormal(temporary, 0); faces.push(face); } catch { /* a degenerate pole or seam */ }
  }
  // Do not retain vertices that appeared only in discarded triangles.
  const used = new Set(faces.flat());
  const remap = new Map();
  const compact = [];
  for (let i = 0; i < vertices.length; i++) if (used.has(i)) { remap.set(i, compact.length); compact.push(vertices[i]); }
  return validateMesh({ vertices: compact, faces: faces.map((face) => face.map((index) => remap.get(index))) });
}

export function moveVertexTo(mesh, vertexIndex, position) {
  if (!Number.isInteger(vertexIndex) || !mesh.vertices[vertexIndex]) throw new Error('Select a vertex first.');
  if (!Array.isArray(position) || position.length !== 3 || position.some((value) => typeof value !== 'number' || !Number.isFinite(value) || Math.abs(value) > MAX_COORDINATE)) throw new Error('Vertex coordinates must be between -1000 and 1000.');
  const result = copyMesh(mesh);
  result.vertices[vertexIndex] = [...position];
  return validateMesh(result);
}
export function moveVertex(mesh, vertexIndex, axis, value) {
  if (!Number.isInteger(vertexIndex) || !mesh.vertices[vertexIndex] || !Number.isInteger(axis) || axis < 0 || axis > 2) throw new Error('Select a vertex first.');
  const position = [...mesh.vertices[vertexIndex]];
  position[axis] = value;
  return moveVertexTo(mesh, vertexIndex, position);
}

export function extrudeFace(mesh, faceIndex, distance) {
  if (!Number.isFinite(distance) || Math.abs(distance) < 0.001 || Math.abs(distance) > 20) throw new Error('Extrusion distance must have a magnitude from 0.001 to 20.');
  const normal = faceNormal(mesh, faceIndex);
  const result = copyMesh(mesh), face = result.faces[faceIndex];
  const top = face.map((index) => {
    const vertex = result.vertices[index].map((value, axis) => value + normal[axis] * distance);
    result.vertices.push(vertex);
    return result.vertices.length - 1;
  });
  result.faces[faceIndex] = top;
  for (let i = 0; i < face.length; i++) {
    const next = (i + 1) % face.length;
    result.faces.push([face[i], face[next], top[next], top[i]]);
  }
  return validateMesh(result);
}

// Inset is a proportional inset toward the face centroid. It preserves the
// outer boundary and adds a quad ring; it is not a constant-distance bevel.
export function insetFace(mesh, faceIndex, amount) {
  if (!Number.isFinite(amount) || amount < 0.01 || amount > 0.9) throw new Error('Inset fraction must be between 0.01 and 0.9.');
  faceNormal(mesh, faceIndex);
  const result = copyMesh(mesh), face = result.faces[faceIndex];
  const center = [0, 0, 0];
  for (const index of face) for (let axis = 0; axis < 3; axis++) center[axis] += result.vertices[index][axis] / face.length;
  const inner = face.map((index) => {
    const vertex = result.vertices[index].map((value, axis) => value + (center[axis] - value) * amount);
    result.vertices.push(vertex);
    return result.vertices.length - 1;
  });
  result.faces[faceIndex] = inner;
  for (let i = 0; i < face.length; i++) {
    const next = (i + 1) % face.length;
    result.faces.push([face[i], face[next], inner[next], inner[i]]);
  }
  return validateMesh(result);
}

// Split every polygon into corner quads. Edge midpoints are shared between
// adjacent faces, so a manifold mesh remains connected without T-junctions.
// This is simple linear subdivision, not Catmull-Clark smoothing.
export function subdivideMesh(mesh) {
  const result = { vertices: mesh.vertices.map((vertex) => [...vertex]), faces: [] };
  const edges = new Map();
  const midpoint = (a, b) => {
    const key = a < b ? `${a}:${b}` : `${b}:${a}`;
    if (!edges.has(key)) {
      if (result.vertices.length >= MAX_MESH_VERTICES) throw new Error('Subdivision exceeds the mesh vertex limit.');
      edges.set(key, result.vertices.length);
      result.vertices.push(mesh.vertices[a].map((value, axis) => (value + mesh.vertices[b][axis]) / 2));
    }
    return edges.get(key);
  };
  for (const face of mesh.faces) {
    if (result.faces.length + face.length > MAX_MESH_FACES || result.vertices.length >= MAX_MESH_VERTICES) throw new Error('Subdivision exceeds the mesh size limit.');
    const center = [0, 0, 0];
    for (const index of face) for (let axis = 0; axis < 3; axis++) center[axis] += mesh.vertices[index][axis] / face.length;
    const centerIndex = result.vertices.length;
    result.vertices.push(center);
    const middle = face.map((vertex, i) => midpoint(vertex, face[(i + 1) % face.length]));
    for (let i = 0; i < face.length; i++) result.faces.push([face[i], middle[i], centerIndex, middle[(i + face.length - 1) % face.length]]);
  }
  return validateMesh(result);
}
