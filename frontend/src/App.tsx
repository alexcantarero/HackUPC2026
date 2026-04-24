import "./App.css";
import { Canvas } from "@react-three/fiber";
import { CameraControls } from "@react-three/drei";
import { useRef, useEffect, useMemo } from "react";
import * as THREE from "three";
import Shelf from "./models/shelf.jsx";
import warehouseOutlineCsv from "../../data/input/Case0/warehouse.csv?raw";

const WORLD_SCALE = 0.004;
const FLOOR_THICKNESS = 0.25;

function parseOutline(input: string): Array<[number, number]> {
  return input
    .split("\n")
    .map((line) => line.trim())
    .filter(Boolean)
    .map((line) => {
      const [xRaw, yRaw] = line.split(",").map((value) => value.trim());
      return [Number(xRaw), Number(yRaw)] as [number, number];
    })
    .filter(([x, y]) => Number.isFinite(x) && Number.isFinite(y));
}

export default function App() {
  const lightRef = useRef<THREE.DirectionalLight>(null);
  const meshRef = useRef<THREE.Mesh>(null);

  const floorGeometry = useMemo(() => {
    const rawPoints = parseOutline(warehouseOutlineCsv);

    // Fallback to a rectangle if input is not enough to build a polygon.
    const points =
      rawPoints.length >= 3
        ? rawPoints
        : [
            [0, 0],
            [10000, 0],
            [10000, 10000],
            [0, 10000],
          ];

    const scaledPoints = points.map(
      ([x, y]) => new THREE.Vector2(x * WORLD_SCALE, y * WORLD_SCALE),
    );

    let minX = Infinity;
    let maxX = -Infinity;
    let minY = Infinity;
    let maxY = -Infinity;

    for (const point of scaledPoints) {
      if (point.x < minX) minX = point.x;
      if (point.x > maxX) maxX = point.x;
      if (point.y < minY) minY = point.y;
      if (point.y > maxY) maxY = point.y;
    }

    const centerX = (minX + maxX) / 2;
    const centerY = (minY + maxY) / 2;
    const centeredPoints = scaledPoints.map(
      (point) => new THREE.Vector2(point.x - centerX, point.y - centerY),
    );

    const shape = new THREE.Shape(centeredPoints);
    const geometry = new THREE.ExtrudeGeometry(shape, {
      depth: FLOOR_THICKNESS,
      bevelEnabled: false,
    });

    geometry.rotateX(-Math.PI / 2);
    geometry.translate(0, -FLOOR_THICKNESS / 2, 0);

    return geometry;
  }, []);

  useEffect(() => {
    return () => {
      floorGeometry.dispose();
    };
  }, [floorGeometry]);

  useEffect(() => {
    if (lightRef.current && meshRef.current) {
      lightRef.current.target.position.copy(meshRef.current.position);
      lightRef.current.target.updateMatrixWorld();
    }
  }, []);

  return (
    <div className="canvas-container">
      <Canvas className="canvas" camera={{ position: [0, 5, 40], fov: 50 }}>
        <color attach="background" args={["#bbbbbb"]} />
        <directionalLight ref={lightRef} position={[0, 5, 5]} intensity={5} />
        <ambientLight intensity={3} />
        <mesh position={[0, -2, 0]} ref={meshRef} geometry={floorGeometry}>
          <meshStandardMaterial color="#a3a3a3" />
        </mesh>
        <Shelf position={[0, -2, 0]} scale={0.05} />
        <CameraControls makeDefault />
      </Canvas>
    </div>
  );
}
