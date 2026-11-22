/* Forma Studio 3D viewport. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { TransformControls } from 'three/addons/controls/TransformControls.js';
import { RoundedBoxGeometry } from 'three/addons/geometries/RoundedBoxGeometry.js';
import { GLTFExporter } from 'three/addons/exporters/GLTFExporter.js';
import { arrayOffsets, evaluatedTransform } from './model.js';
import { createCubeMesh, createPlaneMesh, meshFromTriangles, triangulateFace, proportionalWeights } from './mesh.js';

const radians = (degrees) => degrees * Math.PI / 180;
const degrees = (radiansValue) => radiansValue * 180 / Math.PI;
const clamp = (value, min, max) => Math.max(min, Math.min(max, value));
const VIEW_DIRECTIONS = {
  perspective: [1, 0.75, 1.2], front: [0, 0, 1], back: [0, 0, -1],
  right: [1, 0, 0], left: [-1, 0, 0], top: [0, 1, 0], bottom: [0, -1, 0],
};

function geometryFor(type) {
  switch (type) {
    case 'rounded-box': return new RoundedBoxGeometry(1.4, 1.4, 1.4, 4, 0.16);
    case 'cube': return new THREE.BoxGeometry(1.4, 1.4, 1.4);
    case 'sphere': return new THREE.SphereGeometry(0.8, 36, 24);
    case 'cylinder': return new THREE.CylinderGeometry(0.8, 0.8, 1.4, 48);
    case 'cone': return new THREE.ConeGeometry(0.85, 1.6, 48);
    case 'torus': return new THREE.TorusGeometry(0.8, 0.19, 20, 64);
    case 'plane': { const geometry = new THREE.PlaneGeometry(2, 2); geometry.rotateX(-Math.PI / 2); return geometry; }
    case 'icosahedron': return new THREE.IcosahedronGeometry(0.85, 0);
    default: throw new Error(`Unsupported shape: ${type}`);
  }
}
// Cubes and planes have human-friendly quad faces. Curved primitives retain
// their triangulated surface when converted to an editable mesh.
export function primitiveToMesh(type) {
  if (type === 'cube') return createCubeMesh();
  if (type === 'plane') return createPlaneMesh();
  const geometry = geometryFor(type);
  try {
    const position = geometry.getAttribute('position');
    const vertices = Array.from({ length: position.count }, (_, index) => [position.getX(index), position.getY(index), position.getZ(index)]);
    const indices = geometry.index ? Array.from(geometry.index.array) : Array.from({ length: position.count }, (_, index) => index);
    return meshFromTriangles(vertices, indices);
  } finally { geometry.dispose(); }
}

function editableGeometry(mesh, shading = 'flat') {
  const geometry = new THREE.BufferGeometry();
  const indices = [], faceMap = [];
  for (const [faceIndex] of mesh.faces.entries()) {
    for (const triangle of triangulateFace(mesh, faceIndex)) {
      indices.push(...triangle);
      faceMap.push(faceIndex);
    }
  }
  if (shading === 'smooth') {
    geometry.setAttribute('position', new THREE.Float32BufferAttribute(mesh.vertices.flat(), 3));
    geometry.setIndex(indices);
  } else {
    // Separate corners give each triangle a hard, flat normal.
    geometry.setAttribute('position', new THREE.Float32BufferAttribute(indices.flatMap((index) => mesh.vertices[index]), 3));
  }
  geometry.computeVertexNormals();
  // Three's raycaster reports triangles; map each one back to its polygon.
  geometry.formaFaceMap = faceMap;
  return geometry;
}

function applyTransform(node, value) {
  node.position.fromArray(value.position);
  node.rotation.set(...value.rotation.map(radians));
  node.scale.fromArray(value.scale);
  node.updateMatrixWorld(true);
}
function readTransform(node) {
  const finite = (value, min, max) => clamp(Number.isFinite(value) ? value : 0, min, max);
  return {
    position: node.position.toArray().map((value) => finite(value, -1000, 1000)),
    rotation: [node.rotation.x, node.rotation.y, node.rotation.z].map((value) => finite(degrees(value), -36000, 36000)),
    scale: node.scale.toArray().map((value) => finite(value, 0.01, 100)),
  };
}

export class Viewport {
  constructor(host, project, callbacks = {}) {
    this.host = host;
    this.callbacks = callbacks;
    this.project = project;
    this.frame = 0;
    this.selectedId = null;
    this.tool = 'translate';
    this.wireframe = false;
    this.nodes = new Map();
    this.meshEdit = null;
    this.editOverlay = null;
    this.vertexHandleRoot = null;
    this.vertexHandle = null;
    this.lastValidVertex = null;
    this.proportional = { enabled: false, radius: 1.5, falloff: 'smooth' };
    this.proportionalDragMesh = null;
    this.snapping = false;
    this.dragSnapshot = false;
    this.cameraSettling = false;
    this.capturing = false;

    this.scene = new THREE.Scene();
    this.perspectiveCamera = new THREE.PerspectiveCamera(40, 1, 0.05, 300);
    this.orthographicCamera = new THREE.OrthographicCamera(-1, 1, 1, -1, 0.05, 300);
    this.orthographicHeight = 10;
    this.camera = this.perspectiveCamera;
    this.renderer = new THREE.WebGLRenderer({ antialias: true, alpha: false, preserveDrawingBuffer: true, powerPreference: 'high-performance' });
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 1.5));
    this.renderer.outputColorSpace = THREE.SRGBColorSpace;
    this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
    this.renderer.shadowMap.enabled = true;
    this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    this.renderer.domElement.setAttribute('aria-label', 'Interactive 3D viewport');
    this.renderer.domElement.setAttribute('role', 'img');
    host.append(this.renderer.domElement);
    this.renderer.domElement.addEventListener('webglcontextlost', (event) => { event.preventDefault(); this.callbacks.onError?.('The graphics context was lost. Save your project and restart Forma Studio.'); });

    this.ambient = new THREE.HemisphereLight(0xf3ebff, 0x6c728c, 1.1);
    this.key = new THREE.DirectionalLight(0xffeee3, 2.7);
    this.key.position.set(5, 9, 7);
    this.key.castShadow = true;
    this.key.shadow.mapSize.set(1024, 1024);
    Object.assign(this.key.shadow.camera, { left: -12, right: 12, top: 12, bottom: -12, near: 0.5, far: 35 });
    this.key.shadow.bias = -0.00025;
    this.key.shadow.normalBias = 0.08;
    this.rim = new THREE.DirectionalLight(0xb7a1ff, 1.05);
    this.rim.position.set(-6, 4, -3);
    this.scene.add(this.ambient, this.key, this.rim);
    this.floor = new THREE.Mesh(new THREE.PlaneGeometry(200, 200), new THREE.MeshStandardMaterial({ color: 0x292a36, roughness: 0.96, metalness: 0 }));
    this.floor.rotation.x = -Math.PI / 2;
    this.floor.position.y = -0.16;
    this.floor.receiveShadow = true;
    this.scene.add(this.floor);
    this.grid = new THREE.GridHelper(40, 40, 0x55505f, 0x42424e);
    this.grid.position.y = -0.155;
    this.grid.material.opacity = 0.48;
    this.grid.material.transparent = true;
    this.scene.add(this.grid);
    this.root = new THREE.Group();
    this.root.name = 'Forma scene';
    this.scene.add(this.root);
    this.orbit = new OrbitControls(this.camera, this.renderer.domElement);
    this.orbit.enableDamping = true;
    this.orbit.dampingFactor = 0.1;
    this.orbit.minDistance = 1;
    this.orbit.maxDistance = 100;
    this.orbit.minZoom = 0.02;
    this.orbit.maxZoom = 100;
    this.orbit.maxPolarAngle = Math.PI;
    this.orbit.addEventListener('change', () => this.reportView());
    this.orbit.addEventListener('end', () => { this.reportCamera(); this.cameraSettling = true; });
    this.gizmo = new TransformControls(this.camera, this.renderer.domElement);
    this.gizmo.setSize(0.8);
    this.scene.add(this.gizmo.getHelper());
    this.gizmo.addEventListener('dragging-changed', (event) => { this.orbit.enabled = !event.value; });
    this.gizmo.addEventListener('mouseDown', () => {
      this.dragSnapshot = true;
      // Keep the same influence weights for every update of a single drag.
      const object = this.project.objects.find((item) => item.id === this.meshEdit?.id);
      this.proportionalDragMesh = this.vertexHandle && this.proportional.enabled ? object?.mesh : null;
      this.callbacks.onDragStart?.();
    });
    this.gizmo.addEventListener('objectChange', () => {
      if (!this.dragSnapshot || !this.gizmo.object) return;
      if (this.vertexHandle && this.gizmo.object === this.vertexHandle) {
        let position = this.vertexHandle.position.toArray();
        if (this.snapping) {
          const axes = this.gizmo.axis || 'XYZ';
          position = position.map((value, index) => axes.includes('XYZ'[index]) ? Math.round(value / 0.5) * 0.5 : value);
          this.vertexHandle.position.fromArray(position);
        }
        const mesh = this.callbacks.onVertexDrag?.(position);
        if (mesh) {
          this.lastValidVertex = position;
          this.updateEditableMesh(mesh);
        } else if (this.lastValidVertex) this.vertexHandle.position.fromArray(this.lastValidVertex);
        return;
      }
      const value = readTransform(this.gizmo.object);
      applyTransform(this.gizmo.object, value);
      this.callbacks.onDrag?.(value);
      this.selectionBox?.update();
    });
    this.gizmo.addEventListener('mouseUp', () => {
      if (!this.dragSnapshot) return;
      this.proportionalDragMesh = null;
      this.callbacks.onDragEnd?.();
      // Pointerup can also be a click; do not select through the transform gizmo.
      setTimeout(() => { this.dragSnapshot = false; }, 0);
    });
    this.raycaster = new THREE.Raycaster();
    this.pointer = new THREE.Vector2();
    let down = null;
    this.renderer.domElement.addEventListener('pointerdown', (event) => { if (event.button === 0) down = { x: event.clientX, y: event.clientY }; });
    this.renderer.domElement.addEventListener('pointerup', (event) => {
      if (!down || event.button !== 0) return;
      const moved = Math.hypot(event.clientX - down.x, event.clientY - down.y);
      down = null;
      if (moved > 4 || this.dragSnapshot || this.gizmo.dragging || this.gizmo.axis) return;
      const bounds = this.renderer.domElement.getBoundingClientRect();
      this.pointer.set((event.clientX - bounds.left) / bounds.width * 2 - 1, -((event.clientY - bounds.top) / bounds.height) * 2 + 1);
      this.raycaster.setFromCamera(this.pointer, this.camera);
      if (this.meshEdit) {
        const picked = this.pickMeshComponent(event, bounds);
        this.callbacks.onMeshSelect?.(picked?.index ?? null, picked?.copyIndex ?? 0);
        return;
      }
      const hit = this.raycaster.intersectObjects(this.root.children, true).find((entry) => entry.object.userData.formaId && entry.object.visible && entry.object.parent?.visible);
      this.callbacks.onSelect?.(hit?.object.userData.formaId || null);
    });
    this.setCamera(project.camera);
    this.rebuild(project);
    this.observer = new ResizeObserver(() => this.resize());
    this.observer.observe(host);
    this.resize();
    const tick = (time) => {
      this.callbacks.onTick?.(time);
      const moved = this.orbit.update();
      if (this.cameraSettling && !moved) { this.cameraSettling = false; this.reportCamera(); }
      if (!this.capturing) this.renderer.render(this.scene, this.camera);
      requestAnimationFrame(tick);
    };
    requestAnimationFrame(tick);
  }
  pickMeshComponent(event, bounds) {
    const edit = this.meshEdit;
    const object = this.project.objects.find((item) => item.id === edit?.id && item.type === 'mesh');
    const node = object && this.nodes.get(object.id);
    if (!object || !node?.visible) return null;
    node.updateWorldMatrix(true, true);
    this.camera.updateMatrixWorld();
    if (edit.component === 'face') {
      const hit = this.raycaster.intersectObject(node, true).find((entry) => entry.object.isMesh && Number.isInteger(entry.faceIndex));
      const index = hit?.object.geometry.formaFaceMap[hit.faceIndex];
      return Number.isInteger(index) ? { index, copyIndex: node.children.indexOf(hit.object) } : null;
    }
    // Pick the nearest point in screen space, including the repeated copies.
    // Vertices and edges are deliberately x-ray overlays while editing.
    let best = null, bestDistance = 12 * 12, bestDepth = Infinity;
    const projected = new THREE.Vector3();
    for (const [copyIndex, copy] of node.children.entries()) {
      if (!copy.isMesh) continue;
      for (const [index, vertex] of object.mesh.vertices.entries()) {
        projected.fromArray(vertex).applyMatrix4(copy.matrixWorld).project(this.camera);
        if (projected.z < -1 || projected.z > 1) continue;
        const x = bounds.left + (projected.x + 1) * bounds.width / 2;
        const y = bounds.top + (1 - projected.y) * bounds.height / 2;
        const distance = (x - event.clientX) ** 2 + (y - event.clientY) ** 2;
        if (distance < bestDistance || (distance === bestDistance && projected.z < bestDepth)) {
          best = { index, copyIndex }; bestDistance = distance; bestDepth = projected.z;
        }
      }
    }
    return best;
  }
  clearEditOverlay() {
    if (!this.editOverlay) return;
    this.scene.remove(this.editOverlay);
    const geometries = new Set(), materials = new Set();
    this.editOverlay.traverse((object) => {
      if (object.geometry) geometries.add(object.geometry);
      if (object.material) materials.add(object.material);
    });
    for (const geometry of geometries) geometry.dispose();
    for (const material of materials) material.dispose();
    this.editOverlay = null;
  }
  buildEditOverlay(object, node) {
    const mesh = object.mesh;
    const edgePositions = [], seen = new Set();
    for (const face of mesh.faces) for (let i = 0; i < face.length; i++) {
      const a = face[i], b = face[(i + 1) % face.length];
      const key = a < b ? `${a}:${b}` : `${b}:${a}`;
      if (!seen.has(key)) { seen.add(key); edgePositions.push(...mesh.vertices[a], ...mesh.vertices[b]); }
    }
    const edges = new THREE.BufferGeometry();
    edges.setAttribute('position', new THREE.Float32BufferAttribute(edgePositions, 3));
    const edgeMaterial = new THREE.LineBasicMaterial({ color: 0xbda6ff, transparent: true, opacity: 0.82, depthTest: false });
    let points = null, pointMaterial = null, selectedGeometry = null, selectedMaterial = null;
    let influencedGeometry = null, influencedMaterial = null, radiusGeometry = null, radiusMaterial = null;
    if (this.meshEdit.component === 'vertex') {
      points = new THREE.BufferGeometry();
      points.setAttribute('position', new THREE.Float32BufferAttribute(mesh.vertices.flat(), 3));
      pointMaterial = new THREE.PointsMaterial({ color: 0xf5eefa, size: 7, sizeAttenuation: false, depthTest: false });
      if (Number.isInteger(this.meshEdit.index) && mesh.vertices[this.meshEdit.index]) {
        const active = this.meshEdit.index;
        selectedGeometry = new THREE.BufferGeometry();
        selectedGeometry.setAttribute('position', new THREE.Float32BufferAttribute(mesh.vertices[active], 3));
        selectedMaterial = new THREE.PointsMaterial({ color: 0xffb872, size: 12, sizeAttenuation: false, depthTest: false });
        if (this.proportional.enabled) {
          const { radius, falloff } = this.proportional;
          const weights = proportionalWeights(this.proportionalDragMesh || mesh, active, radius, falloff);
          const positions = [], colors = [];
          weights.forEach((weight, index) => {
            if (index === active || weight <= 0) return;
            positions.push(...mesh.vertices[index]);
            colors.push(1, 0.12 + 0.30 * (1 - weight), 0.02 + 0.18 * (1 - weight));
          });
          if (positions.length) {
            influencedGeometry = new THREE.BufferGeometry();
            influencedGeometry.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
            influencedGeometry.setAttribute('color', new THREE.Float32BufferAttribute(colors, 3));
            influencedMaterial = new THREE.PointsMaterial({ vertexColors: true, size: 9, sizeAttenuation: false, depthTest: false });
          }
          // Three local-space circles describe the spherical influence radius.
          // Object rotation/scale and array offsets transform the preview too.
          const ring = [], center = mesh.vertices[active], segments = 48;
          for (const [a, b] of [[0, 1], [0, 2], [1, 2]]) for (let i = 0; i < segments; i++) {
            for (const angle of [i * Math.PI * 2 / segments, (i + 1) * Math.PI * 2 / segments]) {
              const point = [...center];
              point[a] += radius * Math.cos(angle); point[b] += radius * Math.sin(angle);
              ring.push(...point);
            }
          }
          radiusGeometry = new THREE.BufferGeometry();
          radiusGeometry.setAttribute('position', new THREE.Float32BufferAttribute(ring, 3));
          radiusMaterial = new THREE.LineBasicMaterial({ color: 0xf7b99a, transparent: true, opacity: 0.36, depthTest: false });
        }
      }
    } else if (Number.isInteger(this.meshEdit.index) && mesh.faces[this.meshEdit.index]) {
      const positions = [];
      for (const triangle of triangulateFace(mesh, this.meshEdit.index)) for (const index of triangle) positions.push(...mesh.vertices[index]);
      selectedGeometry = new THREE.BufferGeometry();
      selectedGeometry.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
      selectedMaterial = new THREE.MeshBasicMaterial({ color: 0xffb872, transparent: true, opacity: 0.48, depthTest: false, side: THREE.DoubleSide });
    }
    this.editOverlay = new THREE.Group();
    for (const [copyIndex, offset] of arrayOffsets(object.array).entries()) {
      const copy = new THREE.Group(); copy.position.fromArray(offset);
      if (radiusGeometry && copyIndex === clamp(this.meshEdit.copyIndex || 0, 0, node.children.length - 1)) {
        const radius = new THREE.LineSegments(radiusGeometry, radiusMaterial); radius.renderOrder = 999; copy.add(radius);
      }
      const lines = new THREE.LineSegments(edges, edgeMaterial); lines.renderOrder = 1000; copy.add(lines);
      if (points) { const vertices = new THREE.Points(points, pointMaterial); vertices.renderOrder = 1001; copy.add(vertices); }
      if (influencedGeometry) { const influenced = new THREE.Points(influencedGeometry, influencedMaterial); influenced.renderOrder = 1002; copy.add(influenced); }
      if (selectedGeometry) {
        const selected = this.meshEdit.component === 'vertex' ? new THREE.Points(selectedGeometry, selectedMaterial) : new THREE.Mesh(selectedGeometry, selectedMaterial);
        selected.renderOrder = 1003; copy.add(selected);
      }
      this.editOverlay.add(copy);
    }
    this.syncEditOverlay(node);
    this.scene.add(this.editOverlay);
  }
  syncEditOverlay(node = this.nodes.get(this.meshEdit?.id)) {
    if (!this.editOverlay || !node) return;
    this.editOverlay.position.copy(node.position);
    this.editOverlay.quaternion.copy(node.quaternion);
    this.editOverlay.scale.copy(node.scale);
    this.editOverlay.updateMatrixWorld(true);
  }
  clearVertexHandle() {
    if (this.vertexHandleRoot) this.scene.remove(this.vertexHandleRoot);
    this.vertexHandleRoot = null;
    this.vertexHandle = null;
    this.lastValidVertex = null;
  }
  buildVertexHandle(object, node) {
    const index = this.meshEdit?.index;
    if (!Number.isInteger(index) || !object.mesh.vertices[index]) return;
    const copyIndex = clamp(this.meshEdit.copyIndex || 0, 0, node.children.length - 1);
    this.vertexHandleRoot = new THREE.Group();
    const copy = new THREE.Group();
    copy.position.copy(node.children[copyIndex].position);
    this.vertexHandle = new THREE.Object3D();
    this.vertexHandle.position.fromArray(object.mesh.vertices[index]);
    this.lastValidVertex = [...object.mesh.vertices[index]];
    copy.add(this.vertexHandle); this.vertexHandleRoot.add(copy); this.scene.add(this.vertexHandleRoot);
    this.syncVertexHandle(node);
    this.gizmo.setMode('translate');
    this.gizmo.setSpace('local');
    // Vertex snapping is applied in local coordinates, including free-move
    // handles under rotated/scaled objects; Three's world snap cannot do that.
    this.setSnap(this.snapping);
    this.gizmo.attach(this.vertexHandle);
  }
  syncVertexHandle(node = this.nodes.get(this.meshEdit?.id)) {
    if (!this.vertexHandleRoot || !node) return;
    this.vertexHandleRoot.position.copy(node.position);
    this.vertexHandleRoot.quaternion.copy(node.quaternion);
    this.vertexHandleRoot.scale.copy(node.scale);
    this.vertexHandleRoot.updateMatrixWorld(true);
  }
  updateEditableMesh(mesh) {
    const node = this.nodes.get(this.meshEdit?.id);
    const object = this.project.objects.find((item) => item.id === this.meshEdit?.id);
    if (!node || !object) return;
    const geometry = editableGeometry(mesh, object.shading), previous = new Set();
    for (const copy of node.children) if (copy.isMesh) { previous.add(copy.geometry); copy.geometry = geometry; }
    for (const old of previous) old.dispose();
    // Keep the transform handle alive throughout a drag while refreshing the
    // geometry and the highlighted edges/points on every array copy.
    this.clearEditOverlay();
    this.buildEditOverlay(object, node);
  }
  setMeshEdit(edit, refresh = true) {
    this.meshEdit = edit ? { ...edit } : null;
    if (refresh) this.select(this.selectedId);
  }
  setProportional(value) {
    this.proportional = { ...value };
    if (this.meshEdit?.component === 'vertex') this.select(this.selectedId);
  }
  updateCameraFrusta() {
    const width = this.host.clientWidth, height = this.host.clientHeight;
    if (!width || !height) return;
    const aspect = width / height;
    this.perspectiveCamera.aspect = aspect;
    this.perspectiveCamera.updateProjectionMatrix();
    const halfHeight = this.orthographicHeight / 2;
    Object.assign(this.orthographicCamera, { top: halfHeight, bottom: -halfHeight, left: -halfHeight * aspect, right: halfHeight * aspect });
    this.orthographicCamera.updateProjectionMatrix();
  }
  resize() {
    const width = this.host.clientWidth, height = this.host.clientHeight;
    if (!width || !height) return;
    this.updateCameraFrusta();
    this.renderer.setSize(width, height, false);
  }
  getCamera() {
    const target = this.orbit.target.clone();
    const position = this.camera.position.clone();
    if (this.camera.isOrthographicCamera) {
      // The v2 format stores only a perspective camera pose. Record an
      // equivalent distance so older versions reopen near the same framing.
      const height = (this.camera.top - this.camera.bottom) / this.camera.zoom;
      const distance = clamp(height / (2 * Math.tan(radians(this.perspectiveCamera.fov / 2))), this.orbit.minDistance, this.orbit.maxDistance);
      const direction = position.clone().sub(target).normalize();
      position.copy(target).addScaledVector(direction, distance);
    }
    return { position: position.toArray(), target: target.toArray() };
  }
  reportCamera() { this.callbacks.onCamera?.(this.getCamera()); }
  reportView() {
    const direction = this.camera.position.clone().sub(this.orbit.target).normalize();
    const view = Object.entries(VIEW_DIRECTIONS).find(([, vector]) => direction.dot(new THREE.Vector3(...vector).normalize()) > 1 - 1e-8)?.[0] || 'custom';
    if (view !== this.reportedView) { this.reportedView = view; this.callbacks.onView?.(view); }
  }
  settleOrbit() {
    // Flush residual damping before assigning an exact preset direction.
    this.cameraSettling = false;
    const damping = this.orbit.enableDamping;
    this.orbit.enableDamping = false;
    this.orbit.update();
    this.orbit.enableDamping = damping;
  }
  setCamera(camera) {
    this.settleOrbit();
    this.camera.position.fromArray(camera.position);
    this.orbit.target.fromArray(camera.target);
    if (this.camera.isOrthographicCamera) {
      const distance = clamp(this.camera.position.distanceTo(this.orbit.target), this.orbit.minDistance, this.orbit.maxDistance);
      this.orthographicHeight = 2 * distance * Math.tan(radians(this.perspectiveCamera.fov / 2));
      this.camera.zoom = 1;
      this.updateCameraFrusta();
    }
    this.orbit.update();
    this.reportView();
  }
  setProjection(projection, report = true) {
    const orthographic = projection === 'orthographic';
    if (!orthographic && projection !== 'perspective') return;
    if (this.camera.isOrthographicCamera === orthographic) return;
    this.settleOrbit();
    const previous = this.camera;
    const distance = previous.position.distanceTo(this.orbit.target);
    if (orthographic) {
      // Match the visible height at the target plane to avoid a visual jump.
      this.orthographicHeight = 2 * distance * Math.tan(radians(this.perspectiveCamera.fov / 2)) / previous.zoom;
      this.orthographicCamera.zoom = 1;
      this.orthographicCamera.position.copy(previous.position);
      this.camera = this.orthographicCamera;
    } else {
      const height = (previous.top - previous.bottom) / previous.zoom;
      const equivalentDistance = height / (2 * Math.tan(radians(this.perspectiveCamera.fov / 2)));
      const direction = previous.position.clone().sub(this.orbit.target).normalize();
      this.perspectiveCamera.position.copy(this.orbit.target).addScaledVector(direction, clamp(equivalentDistance, this.orbit.minDistance, this.orbit.maxDistance));
      this.camera = this.perspectiveCamera;
    }
    this.camera.up.copy(previous.up);
    this.camera.quaternion.copy(previous.quaternion);
    this.updateCameraFrusta();
    this.orbit.object = this.camera;
    this.gizmo.camera = this.camera;
    this.orbit.update();
    this.callbacks.onProjection?.(orthographic ? 'orthographic' : 'perspective');
    this.reportView();
    if (report) this.reportCamera();
  }
  setWorld(world) {
    this.scene.background = new THREE.Color(world.background);
    this.ambient.intensity = world.ambient;
    this.key.intensity = world.key;
    this.renderer.toneMappingExposure = world.exposure;
  }
  rebuild(project) {
    this.project = project;
    this.gizmo.detach();
    if (this.selectionBox) { this.scene.remove(this.selectionBox); this.selectionBox.geometry.dispose(); this.selectionBox.material.dispose(); this.selectionBox = null; }
    const geometries = new Set(), materials = new Set();
    this.root.traverse((object) => { if (object.isMesh) { geometries.add(object.geometry); materials.add(object.material); } });
    this.root.clear();
    for (const geometry of geometries) geometry.dispose();
    for (const material of materials) material.dispose();
    this.nodes.clear();
    for (const object of project.objects) {
      const group = new THREE.Group();
      group.name = object.name;
      group.visible = object.visible;
      group.userData.formaId = object.id;
      const geometry = object.type === 'mesh' ? editableGeometry(object.mesh, object.shading) : geometryFor(object.type);
      const material = new THREE.MeshStandardMaterial({
        color: object.material.color, roughness: object.material.roughness, metalness: object.material.metalness,
        side: object.type === 'plane' || object.type === 'mesh' ? THREE.DoubleSide : THREE.FrontSide,
        flatShading: object.type === 'mesh' && object.shading !== 'smooth', wireframe: this.wireframe,
      });
      for (const offset of arrayOffsets(object.array)) {
        const mesh = new THREE.Mesh(geometry, material);
        mesh.position.fromArray(offset);
        mesh.castShadow = true;
        mesh.receiveShadow = true;
        mesh.userData.formaId = object.id;
        group.add(mesh);
      }
      applyTransform(group, evaluatedTransform(object, this.frame));
      this.root.add(group);
      this.nodes.set(object.id, group);
    }
    this.setWorld(project.world);
    this.select(this.selectedId);
  }
  select(id) {
    this.selectedId = id;
    this.gizmo.detach();
    this.clearVertexHandle();
    this.setSnap(this.snapping);
    this.clearEditOverlay();
    if (this.selectionBox) { this.scene.remove(this.selectionBox); this.selectionBox.geometry.dispose(); this.selectionBox.material.dispose(); this.selectionBox = null; }
    const node = this.nodes.get(id);
    if (!node || !node.visible) return;
    const object = this.project.objects.find((item) => item.id === id);
    if (this.meshEdit?.id === id && object?.type === 'mesh') {
      this.buildEditOverlay(object, node);
      if (this.meshEdit.component === 'vertex') this.buildVertexHandle(object, node);
      return;
    }
    this.gizmo.setSpace('world');
    this.selectionBox = new THREE.BoxHelper(node, 0xc9acf9);
    this.selectionBox.material.depthTest = false;
    this.selectionBox.material.transparent = true;
    this.selectionBox.material.opacity = 0.75;
    this.selectionBox.renderOrder = 999;
    this.scene.add(this.selectionBox);
    if (this.tool !== 'select') { this.gizmo.setMode(this.tool); this.gizmo.attach(node); }
  }
  setTool(tool) { this.tool = tool; this.select(this.selectedId); }
  setFrame(frame) {
    this.frame = frame;
    if (this.gizmo.dragging) return;
    for (const object of this.project.objects) {
      const node = this.nodes.get(object.id);
      if (node) applyTransform(node, evaluatedTransform(object, frame));
    }
    this.selectionBox?.update();
    this.syncEditOverlay();
    this.syncVertexHandle();
  }
  setSnap(value) {
    this.snapping = Boolean(value);
    this.gizmo.setTranslationSnap(this.snapping && !this.vertexHandle ? 0.5 : null);
    this.gizmo.setRotationSnap(this.snapping ? radians(15) : null);
    this.gizmo.setScaleSnap(this.snapping ? 0.1 : null);
  }
  setWireframe(value) {
    this.wireframe = value;
    this.root.traverse((object) => { if (object.isMesh) object.material.wireframe = value; });
  }
  setGrid(value) { this.grid.visible = value; }
  frameSelected() {
    const node = this.nodes.get(this.selectedId);
    if (!node || !node.visible) return false;
    const box = new THREE.Box3().setFromObject(node);
    if (box.isEmpty()) return false;
    this.settleOrbit();
    const center = box.getCenter(new THREE.Vector3());
    const size = box.getSize(new THREE.Vector3());
    const distance = Math.max(size.x, size.y, size.z, 1) * 2.6;
    const direction = this.camera.position.clone().sub(this.orbit.target).normalize();
    if (this.camera.isOrthographicCamera) {
      // Fit the projected bounding box in both viewport dimensions. Zooming
      // an orthographic camera changes its frustum, not its distance.
      const right = new THREE.Vector3(1, 0, 0).applyQuaternion(this.camera.quaternion);
      const up = new THREE.Vector3(0, 1, 0).applyQuaternion(this.camera.quaternion);
      const corners = [];
      for (const x of [box.min.x, box.max.x]) for (const y of [box.min.y, box.max.y]) for (const z of [box.min.z, box.max.z]) corners.push(new THREE.Vector3(x, y, z));
      const horizontal = corners.map((corner) => corner.dot(right));
      const vertical = corners.map((corner) => corner.dot(up));
      const aspect = this.host.clientWidth / Math.max(this.host.clientHeight, 1);
      const width = Math.max(...horizontal) - Math.min(...horizontal);
      const height = Math.max(...vertical) - Math.min(...vertical);
      this.orthographicHeight = Math.max(height, width / Math.max(aspect, 0.001), 0.1) * 1.35;
      this.camera.zoom = 1;
      this.updateCameraFrusta();
    }
    this.orbit.target.copy(center);
    this.camera.position.copy(center).addScaledVector(direction, distance);
    this.orbit.update();
    this.reportCamera();
    return true;
  }
  setView(view) {
    const direction = VIEW_DIRECTIONS[view];
    if (!direction) return;
    this.settleOrbit();
    this.setProjection(view === 'perspective' ? 'perspective' : 'orthographic', false);
    const distance = Math.max(this.camera.position.distanceTo(this.orbit.target), 3);
    this.camera.position.copy(this.orbit.target).add(new THREE.Vector3(...direction).normalize().multiplyScalar(distance));
    this.orbit.update();
    this.reportView();
    this.reportCamera();
  }
  async exportGlb() {
    // The export is the current frame only. Studio helpers, floor, lights,
    // camera, and timeline are intentionally not part of the interchange.
    const copy = this.root.clone(true);
    copy.traverse((object) => {
      if (object.isMesh) { object.material = object.material.clone(); object.material.wireframe = false; }
    });
    try { return await new GLTFExporter().parseAsync(copy, { binary: true, onlyVisible: true, trs: true }); }
    finally { copy.traverse((object) => { if (object.isMesh) object.material.dispose(); }); }
  }
  async renderPng() {
    this.capturing = true;
    const gridVisible = this.grid.visible;
    const boxVisible = this.selectionBox?.visible;
    const editVisible = this.editOverlay?.visible;
    const helper = this.gizmo.getHelper();
    const gizmoVisible = helper.visible;
    try {
      this.grid.visible = false;
      if (this.selectionBox) this.selectionBox.visible = false;
      if (this.editOverlay) this.editOverlay.visible = false;
      helper.visible = false;
      this.renderer.render(this.scene, this.camera);
      const blob = await new Promise((resolve, reject) => this.renderer.domElement.toBlob((value) => value ? resolve(value) : reject(new Error('Could not create PNG.')), 'image/png'));
      return await blob.arrayBuffer();
    } finally {
      this.grid.visible = gridVisible;
      if (this.selectionBox) this.selectionBox.visible = boxVisible;
      if (this.editOverlay) this.editOverlay.visible = editVisible;
      helper.visible = gizmoVisible;
      this.capturing = false;
    }
  }
}
