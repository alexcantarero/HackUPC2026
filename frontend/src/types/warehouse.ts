import * as THREE from "three";

export interface WarehouseLayoutItem {
  id: number;
  x: number;
  y: number;
  rot: number;
}

export interface BayType {
  width: number;
  depth: number;
  height: number;
  gap: number;
}

export interface WarehouseCenter {
  centerX: number;
  centerY: number;
}

export interface SceneGeometry {
  floor: THREE.BufferGeometry;
  ceilingParts: Array<{ geometry: THREE.BufferGeometry; y: number }>;
  ceilingWalls: Array<{ geometry: THREE.BufferGeometry; topY: number }>;
  walls: THREE.BufferGeometry[];
  obstacles: Array<{ geometry: THREE.BufferGeometry; position: [number, number, number] }>;
}
