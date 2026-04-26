import React, { Suspense } from "react";
import * as THREE from "three";
import { CameraControls } from "@react-three/drei";
import FadingDollhouseElement from "../FadingDollhouseElement";
import Warehouse from "./Warehouse";
import BayInspector from "./BayInspector";
import { FLOOR_Y } from "../../scene/buildSceneGeometry";
import type { SceneGeometry, WarehouseLayoutItem, BayType, WarehouseCenter } from "../../types/warehouse";

interface WarehouseSceneProps {
  sceneGeometry: SceneGeometry;
  layoutData: WarehouseLayoutItem[];
  bayData: Record<number, BayType>;
  warehouseCenter: WarehouseCenter;
  showGaps: boolean;
  selectedBay: { id: number; nLoads: number; price: number; position: [number, number, number] } | null;
  onSelectBay: (bay: { id: number; nLoads: number; price: number; position: [number, number, number] } | null) => void;
  lightRef: React.RefObject<THREE.DirectionalLight | null>;
  meshRef: React.RefObject<THREE.Mesh | null>;
  cameraControlsRef: React.RefObject<CameraControls | null>;
}

export default function WarehouseScene({
  sceneGeometry,
  layoutData,
  bayData,
  warehouseCenter,
  showGaps,
  selectedBay,
  onSelectBay,
  lightRef,
  meshRef,
  cameraControlsRef,
}: WarehouseSceneProps) {
  return (
    <Suspense fallback={null}>
      <BayInspector selectedBay={selectedBay} />
      <color attach="background" args={["#7e7e7e"]} />
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
        <mesh key={`wall-${index}`} position={[0, FLOOR_Y, 0]} geometry={wall}>
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

      <Warehouse
        layoutData={layoutData}
        bayData={bayData}
        warehouseCenter={warehouseCenter}
        showGaps={showGaps}
        onSelectBay={onSelectBay}
      />

      <CameraControls ref={cameraControlsRef} makeDefault />
    </Suspense>
  );
}
