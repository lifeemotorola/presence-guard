/* Forma Studio mesh tests. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
import test from 'node:test';
import assert from 'node:assert/strict';
import { createCubeMesh, createPlaneMesh, validateMesh, faceNormal, triangulateFace, extrudeFace, insetFace, subdivideMesh, moveVertex, proportionalWeights, moveVertexProportionally, meshFromTriangles, MAX_MESH_VERTICES } from '../app/mesh.js';
import { createStarterProject, createObject, validateProject, VERSION, PRIMITIVE_TYPES } from '../app/model.js';
import { primitiveToMesh } from '../app/viewport.js';

function edgeUse(mesh) {
  const edges = new Map();
  for (const face of mesh.faces) for (let i = 0; i < face.length; i++) {
    const a = face[i], b = face[(i + 1) % face.length];
    const key = a < b ? `${a}:${b}` : `${b}:${a}`;
    edges.set(key, (edges.get(key) || 0) + 1);
  }
  return [...edges.values()];
}

test('version 1 projects migrate without losing their primitive scene data', () => {
  const project = createStarterProject();
  const legacy = JSON.parse(JSON.stringify(project)); legacy.version = 1;
  assert.equal(VERSION, 2);
  assert.deepEqual(validateProject(legacy), project);
  assert.throws(() => validateProject({ ...legacy, version: 3 }), /unsupported format or version/);
  const bad = createObject('cube', 'one'); bad.type = 'mesh'; bad.mesh = createCubeMesh();
  assert.throws(() => validateProject({ ...legacy, objects: [bad] }), /unknown object type/);
});

test('editable meshes round-trip and malformed topology is rejected', () => {
  const project = createStarterProject(), object = createObject('cube', 'mesh-one');
  object.type = 'mesh'; object.mesh = createCubeMesh(); object.shading = 'smooth'; project.objects.push(object);
  assert.deepEqual(validateProject(JSON.parse(JSON.stringify(project))), project);
  assert.throws(() => validateProject({ ...project, objects: [{ ...object, shading: 'unknown' }] }), /mesh shading/);
  assert.equal(validateProject({ ...project, objects: [{ ...object, shading: undefined }] }).objects[0].shading, 'flat');
  for (const mesh of [
    { vertices: [], faces: [] },
    { ...object.mesh, vertices: [[NaN, 0, 0], ...object.mesh.vertices.slice(1)] },
    { ...object.mesh, faces: [[0, 1, 9]] },
    { ...object.mesh, faces: [[0, 1, 1]] },
    { ...object.mesh, faces: [[0, 1]] },
    { ...object.mesh, faces: [[0, 1, 2, 3, 4, 5, 6, 7, 0]] },
    { vertices: [[0, 0, 0], [1, 0, 0], [2, 0, 0]], faces: [[0, 1, 2]] },
    { vertices: new Array(MAX_MESH_VERTICES + 1).fill([0, 0, 0]), faces: [[0, 1, 2]] },
  ]) assert.throws(() => validateProject({ ...project, objects: [{ ...object, mesh }] }), /Invalid Forma project/);
});

test('face normals point outward and extrude adds connected walls', () => {
  const cube = createCubeMesh();
  assert.deepEqual(faceNormal(cube, 1), [0, 0, 1]);
  assert.deepEqual(faceNormal(cube, 3), [0, 1, 0]);
  const extruded = extrudeFace(cube, 1, 2);
  assert.equal(cube.vertices.length, 8, 'the original must not be mutated');
  assert.equal(extruded.vertices.length, 12);
  assert.equal(extruded.faces.length, 10);
  for (const index of extruded.faces[1]) assert.ok(Math.abs(extruded.vertices[index][2] - 2.7) < 1e-10);
  assert.ok(edgeUse(extruded).every((count) => count === 2), 'closed cube remains watertight after extrusion');
  assert.throws(() => extrudeFace(cube, 1, 0), /distance/);
  assert.throws(() => extrudeFace(cube, 99, 1), /Select a face/);
});

test('inset creates an inner face and a connected border', () => {
  const cube = createCubeMesh(), inset = insetFace(cube, 1, 0.5);
  assert.equal(inset.vertices.length, 12);
  assert.equal(inset.faces.length, 10);
  for (const index of inset.faces[1]) {
    const [x, y, z] = inset.vertices[index];
    assert.ok(Math.abs(Math.abs(x) - 0.35) < 1e-9 && Math.abs(Math.abs(y) - 0.35) < 1e-9 && Math.abs(z - 0.7) < 1e-9);
  }
  assert.ok(edgeUse(inset).every((count) => count === 2));
  assert.throws(() => insetFace(cube, 1, 1), /Inset fraction/);
});

test('linear subdivision shares edge midpoints and vertex edits are validated', () => {
  const cube = createCubeMesh(), subdivided = subdivideMesh(cube);
  assert.equal(subdivided.vertices.length, 26);
  assert.equal(subdivided.faces.length, 24);
  assert.ok(edgeUse(subdivided).every((count) => count === 2));
  const moved = moveVertex(cube, 0, 1, -2);
  assert.equal(moved.vertices[0][1], -2);
  assert.equal(cube.vertices[0][1], -0.7);
  assert.throws(() => moveVertex(cube, 0, 1, Infinity), /coordinates/);
  assert.throws(() => moveVertex(cube, 0, 1, 1001), /coordinates/);
  assert.throws(() => moveVertex(cube, 500, 1, 0), /Select a vertex/);
});

test('proportional movement uses local distance, selectable falloff and the original mesh', () => {
  const mesh = subdivideMesh(createPlaneMesh());
  const center = 4, edge = 5, corner = 0;
  const original = JSON.parse(JSON.stringify(mesh));
  assert.deepEqual(mesh.vertices[center], [0, 0, 0]);
  const linear = proportionalWeights(mesh, center, 2, 'linear');
  const smooth = proportionalWeights(mesh, center, 2, 'smooth');
  const sharp = proportionalWeights(mesh, center, 2, 'sharp');
  assert.equal(linear[center], 1);
  assert.equal(linear[edge], 0.5);
  assert.equal(smooth[edge], 0.5);
  assert.equal(sharp[edge], 0.25);
  assert.ok(smooth[corner] < linear[corner] && sharp[corner] < smooth[corner]);
  assert.equal(proportionalWeights(mesh, center, 1, 'linear')[edge], 0, 'vertices on the radius boundary do not move');
  const moved = moveVertexProportionally(mesh, center, [0, 2, 0], 2, 'linear');
  assert.equal(moved.vertices[center][1], 2);
  assert.equal(moved.vertices[edge][1], 1);
  assert.ok(Math.abs(moved.vertices[corner][1] - 2 * (1 - Math.SQRT2 / 2)) < 1e-12);
  assert.deepEqual(mesh, original, 'the original mesh is not mutated');
  assert.deepEqual(moved.faces, mesh.faces, 'topology is unchanged');
  const finalDirect = moveVertexProportionally(mesh, center, [0, 0.8, 0], 2, 'smooth');
  const preview = moveVertexProportionally(mesh, center, [0, 0.4, 0], 2, 'smooth');
  assert.notDeepEqual(preview.vertices, finalDirect.vertices);
  assert.deepEqual(moveVertexProportionally(mesh, center, [0, 0.8, 0], 2, 'smooth'), finalDirect, 'repeated drag updates can use the unmodified baseline');
});

test('proportional influence includes nearby disconnected vertices in local space', () => {
  const mesh = createPlaneMesh();
  mesh.vertices.push([-0.9, 0.1, -1], [-0.8, 0.1, -1], [-0.9, 0.1, -0.9]);
  mesh.faces.push([4, 5, 6]);
  const weights = proportionalWeights(mesh, 0, 0.5, 'linear');
  assert.ok(weights[4] > 0 && weights[4] < 1);
  const result = moveVertexProportionally(mesh, 0, [-1, 0.2, -1], 0.5, 'linear');
  assert.ok(result.vertices[4][1] > 0.1, 'a nearby but disconnected surface also moves');
  assert.deepEqual(result.vertices[2], mesh.vertices[2], 'points outside the radius remain unchanged');
});

test('proportional edits reject invalid input and invalid resulting geometry atomically', () => {
  const mesh = subdivideMesh(createPlaneMesh());
  const before = JSON.stringify(mesh);
  for (const radius of [0, 0.009, 1001, Infinity, NaN]) assert.throws(() => proportionalWeights(mesh, 4, radius), /radius/);
  assert.throws(() => proportionalWeights(mesh, 99, 2), /Select a vertex/);
  assert.throws(() => proportionalWeights(mesh, 4, 2, 'random'), /falloff/);
  assert.throws(() => moveVertexProportionally(mesh, 4, [0, Infinity, 0], 2), /coordinates/);
  assert.throws(() => moveVertexProportionally(mesh, 4, [1001, 0, 0], 2), /coordinates/);
  assert.throws(() => moveVertexProportionally(mesh, 4, mesh.vertices[5], 0.5), /degenerate|self-intersecting/);
  assert.equal(JSON.stringify(mesh), before);
  // A neighboring vertex can exceed the coordinate limit even if the active
  // vertex's destination is itself in range.
  const far = { vertices: [[999, 0, 0], [1000, 0, 0], [999, 1, 0]], faces: [[0, 1, 2]] };
  assert.throws(() => moveVertexProportionally(far, 0, [1000, 0, 0], 2), /outside/);
  assert.deepEqual(far.vertices[1], [1000, 0, 0]);
});

test('every built-in primitive converts to bounded editable geometry', () => {
  for (const type of PRIMITIVE_TYPES) {
    const mesh = primitiveToMesh(type);
    assert.deepEqual(validateMesh(mesh), mesh, type);
    assert.ok(mesh.vertices.length >= 4 && mesh.faces.length >= 1, type);
  }
  assert.equal(primitiveToMesh('cube').faces.length, 6);
  assert.equal(primitiveToMesh('plane').faces.length, 1);
});

test('concave polygons triangulate without spilling outside the face, while crossing faces are rejected', () => {
  const mesh = { vertices: [[0, 0, 0], [3, 0, 0], [3, 3, 0], [1.5, 1, 0], [0, 3, 0]], faces: [[0, 1, 2, 3, 4]] };
  const triangles = triangulateFace(mesh, 0);
  assert.equal(triangles.length, 3);
  const area = (indices) => {
    const [a, b, c] = indices.map((index) => mesh.vertices[index]);
    return ((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) / 2;
  };
  assert.ok(triangles.every((triangle) => area(triangle) > 0));
  assert.equal(triangles.reduce((sum, triangle) => sum + area(triangle), 0), 6);
  assert.deepEqual(validateMesh(mesh), mesh);
  const reverse = { ...mesh, faces: [[4, 3, 2, 1, 0]] };
  assert.ok(triangulateFace(reverse, 0).every((triangle) => area(triangle) < 0));
  const crossing = { vertices: [[0, 0, 0], [3, 3, 0], [0, 3, 0], [2, 0, 0]], faces: [[0, 1, 2, 3]] };
  assert.throws(() => validateMesh(crossing), /self-intersecting/);
});

test('mesh and whole-project budgets refuse oversized edits without changing the original', () => {
  const torus = primitiveToMesh('torus');
  assert.throws(() => subdivideMesh(torus), /limit/);
  assert.equal(torus.vertices.length, 1280);
  assert.equal(torus.faces.length, 2560);
  const project = createStarterProject();
  const objects = Array.from({ length: 19 }, (_, index) => {
    const object = createObject('torus', `torus-${index}`);
    object.type = 'mesh'; object.mesh = torus; object.shading = 'smooth';
    return object;
  });
  assert.equal(validateProject({ ...project, objects: objects.slice(0, 18) }).objects.length, 18);
  assert.throws(() => validateProject({ ...project, objects }), /total editable mesh size/);
});

test('triangle conversion welds seams and drops degenerate faces', () => {
  const positions = [[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 0], [1, 0, 0]];
  const mesh = meshFromTriangles(positions, [0, 1, 2, 3, 4, 2, 0, 3, 1]);
  assert.equal(mesh.vertices.length, 3);
  assert.equal(mesh.faces.length, 2);
  assert.deepEqual(mesh.faces[0], mesh.faces[1]);
  assert.deepEqual(validateMesh(createPlaneMesh()), createPlaneMesh());
});
