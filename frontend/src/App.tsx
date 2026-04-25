import "./App.css";
import { Canvas } from "@react-three/fiber";
import { CameraControls } from "@react-three/drei";
import { useEffect, useMemo, useRef } from "react";
import * as THREE from "three";
import Shelf from "./models/shelf.jsx";
import FadingDollhouseElement from "./components/FadingDollhouseElement";
import { buildSceneGeometry, FLOOR_Y } from "./scene/buildSceneGeometry";

const CASE_NUMBER = 0;

const warehouseFiles = import.meta.glob(
  "../../data/input/Case*/warehouse.csv",
  { query: "?raw", import: "default", eager: true },
) as Record<string, string>;
const ceilingFiles = import.meta.glob("../../data/input/Case*/ceiling.csv", {
  query: "?raw",
  import: "default",
  eager: true,
}) as Record<string, string>;
const obstacleFiles = import.meta.glob("../../data/input/Case*/obstacles.csv", {
  query: "?raw",
  import: "default",
  eager: true,
}) as Record<string, string>;

function getCaseCsv(files: Record<string, string>, kind: string) {
  const filePath = `../../data/input/Case${CASE_NUMBER}/${kind}.csv`;
  return files[filePath] ?? "";
}

export default function App() {
  const lightRef = useRef<THREE.DirectionalLight>(null);
  const meshRef = useRef<THREE.Mesh>(null);

  const sceneGeometry = useMemo(
    () =>
      buildSceneGeometry({
        warehouseOutlineCsv: getCaseCsv(warehouseFiles, "warehouse"),
        ceilingCsv: getCaseCsv(ceilingFiles, "ceiling"),
        obstaclesCsv: getCaseCsv(obstacleFiles, "obstacles"),
      }),
    [],
  );

  useEffect(() => {
    return () => {
      sceneGeometry.floor.dispose();
      for (const part of sceneGeometry.ceilingParts) part.geometry.dispose();
      for (const cw of sceneGeometry.ceilingWalls) cw.geometry.dispose();
      for (const wall of sceneGeometry.walls) wall.dispose();
      for (const obstacle of sceneGeometry.obstacles)
        obstacle.geometry.dispose();
    };
  }, [sceneGeometry]);

  useEffect(() => {
    if (lightRef.current && meshRef.current) {
      lightRef.current.target.position.copy(meshRef.current.position);
      lightRef.current.target.updateMatrixWorld();
    }
  }, []);

  return (
    <div className="canvas-container">
      <Canvas
        className="canvas"
        camera={{ position: [0, 5, 100], rotation: [0, 45, 0], fov: 50 }}
      >
        <color attach="background" args={["#878787"]} />
        <directionalLight ref={lightRef} position={[0, 5, 5]} intensity={5} />
        <ambientLight intensity={3} />

        <mesh
          position={[0, FLOOR_Y, 0]}
          ref={meshRef}
          geometry={sceneGeometry.floor}
        >
          <meshStandardMaterial color="#a3a3a3" />
        </mesh>

        {sceneGeometry.ceilingParts.map((part, index) => (
          <FadingDollhouseElement
            key={`ceiling-${index}`}
            geometry={part.geometry}
            positionY={FLOOR_Y + part.y}
            thresholdY={part.y}
          />
        ))}

        {sceneGeometry.ceilingWalls.map((cw, index) => (
          <FadingDollhouseElement
            key={`ceiling-wall-${index}`}
            geometry={cw.geometry}
            positionY={FLOOR_Y}
            thresholdY={cw.topY}
          />
        ))}

        {sceneGeometry.walls.map((wall, index) => (
          <mesh
            key={`wall-${index}`}
            position={[0, FLOOR_Y, 0]}
            geometry={wall}
          >
            <meshStandardMaterial color="#c4c4c4" side={THREE.BackSide} />
          </mesh>
        ))}

        {sceneGeometry.obstacles.map((obstacle, index) => (
          <mesh
            key={`obstacle-${index}`}
            position={[
              obstacle.position[0],
              FLOOR_Y + obstacle.position[1],
              obstacle.position[2],
            ]}
            geometry={obstacle.geometry}
          >
            <meshStandardMaterial color="#ec8200" side={THREE.FrontSide} />
          </mesh>
        ))}

        <Shelf position={[0, FLOOR_Y, 0]} scale={0.05} />
        <CameraControls makeDefault />
      </Canvas>
    </div>
  );
}
