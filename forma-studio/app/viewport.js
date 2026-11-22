/* Forma Studio 3D viewport. Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later. */
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { TransformControls } from 'three/addons/controls/TransformControls.js';
import { RoundedBoxGeometry } from 'three/addons/geometries/RoundedBoxGeometry.js';
import { GLTFExporter } from 'three/addons/exporters/GLTFExporter.js';
import { arrayOffsets, evaluatedTransform } from './model.js';

const radians = (degrees) => degrees * Math.PI / 180;
const degrees = (radiansValue) => radiansValue * 180 / Math.PI;
const clamp = (value, min, max) => Math.max(min, Math.min(max, value));

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
    this.dragSnapshot = false;
    this.capturing = false;

    this.scene = new THREE.Scene();
    this.camera = new THREE.PerspectiveCamera(40, 1, 0.05, 300);
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
    this.orbit.maxPolarAngle = Math.PI / 2 + 0.35;
    this.orbit.addEventListener('end', () => this.reportCamera());
    this.gizmo = new TransformControls(this.camera, this.renderer.domElement);
    this.gizmo.setSize(0.8);
    this.scene.add(this.gizmo.getHelper());
    this.gizmo.addEventListener('dragging-changed', (event) => { this.orbit.enabled = !event.value; });
    this.gizmo.addEventListener('mouseDown', () => { this.dragSnapshot = true; this.callbacks.onDragStart?.(); });
    this.gizmo.addEventListener('objectChange', () => {
      if (!this.dragSnapshot || !this.gizmo.object) return;
      const value = readTransform(this.gizmo.object);
      applyTransform(this.gizmo.object, value);
      this.callbacks.onDrag?.(value);
      this.selectionBox?.update();
    });
    this.gizmo.addEventListener('mouseUp', () => {
      if (!this.dragSnapshot) return;
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
      this.orbit.update();
      if (!this.capturing) this.renderer.render(this.scene, this.camera);
      requestAnimationFrame(tick);
    };
    requestAnimationFrame(tick);
  }
  resize() {
    const width = this.host.clientWidth, height = this.host.clientHeight;
    if (!width || !height) return;
    this.camera.aspect = width / height;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(width, height, false);
  }
  getCamera() { return { position: this.camera.position.toArray(), target: this.orbit.target.toArray() }; }
  reportCamera() {
    this.callbacks.onCamera?.(this.getCamera());
  }
  setCamera(camera) {
    this.camera.position.fromArray(camera.position);
    this.orbit.target.fromArray(camera.target);
    this.orbit.update();
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
      const geometry = geometryFor(object.type);
      const material = new THREE.MeshStandardMaterial({ color: object.material.color, roughness: object.material.roughness, metalness: object.material.metalness, side: object.type === 'plane' ? THREE.DoubleSide : THREE.FrontSide, wireframe: this.wireframe });
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
    if (this.selectionBox) { this.scene.remove(this.selectionBox); this.selectionBox.geometry.dispose(); this.selectionBox.material.dispose(); this.selectionBox = null; }
    const node = this.nodes.get(id);
    if (!node || !node.visible) return;
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
    const center = box.getCenter(new THREE.Vector3());
    const size = box.getSize(new THREE.Vector3());
    const distance = Math.max(size.x, size.y, size.z, 1) * 2.6;
    const direction = this.camera.position.clone().sub(this.orbit.target).normalize();
    this.orbit.target.copy(center);
    this.camera.position.copy(center).addScaledVector(direction, distance);
    this.orbit.update();
    this.reportCamera();
    return true;
  }
  setView(view) {
    const distance = Math.max(this.camera.position.distanceTo(this.orbit.target), 3);
    const direction = {
      perspective: [1, 0.75, 1.2], front: [0, 0.001, 1], top: [0, 1, 0.001], right: [1, 0.001, 0],
    }[view] || [1, 0.75, 1.2];
    this.camera.position.copy(this.orbit.target).add(new THREE.Vector3(...direction).normalize().multiplyScalar(distance));
    this.orbit.update();
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
    const helper = this.gizmo.getHelper();
    const gizmoVisible = helper.visible;
    try {
      this.grid.visible = false;
      if (this.selectionBox) this.selectionBox.visible = false;
      helper.visible = false;
      this.renderer.render(this.scene, this.camera);
      const blob = await new Promise((resolve, reject) => this.renderer.domElement.toBlob((value) => value ? resolve(value) : reject(new Error('Could not create PNG.')), 'image/png'));
      return await blob.arrayBuffer();
    } finally {
      this.grid.visible = gridVisible;
      if (this.selectionBox) this.selectionBox.visible = boxVisible;
      helper.visible = gizmoVisible;
      this.capturing = false;
    }
  }
}
