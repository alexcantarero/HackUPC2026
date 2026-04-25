import { useEffect, useRef } from "react";
import * as THREE from "three";
import { useFrame } from "@react-three/fiber";

import { FLOOR_Y } from "../scene/buildSceneGeometry";

interface FadingDollhouseElementProps {
  geometry: THREE.BufferGeometry;
  positionY: number;
  thresholdY: number;
  color?: string;
  side?: THREE.Side;
}

export default function FadingDollhouseElement({
  geometry,
  positionY,
  thresholdY,
  color = "#d6d6d6",
  side = THREE.DoubleSide,
}: FadingDollhouseElementProps) {
  const materialRef = useRef<THREE.MeshStandardMaterial>(null);

  useFrame(({ camera }, delta) => {
    if (!materialRef.current) return;

    const targetOpacity = camera.position.y < FLOOR_Y + thresholdY ? 1 : 0;
    materialRef.current.opacity = THREE.MathUtils.damp(
      materialRef.current.opacity,
      targetOpacity,
      4,
      delta,
    );
  });

  useEffect(() => {
    if (materialRef.current) {
      materialRef.current.opacity = 0;
    }
  }, []);

  return (
    <mesh position={[0, positionY, 0]} geometry={geometry} castShadow receiveShadow>
      <meshStandardMaterial
        ref={materialRef}
        color={color}
        side={side}
        transparent
      />
    </mesh>
  );
}
