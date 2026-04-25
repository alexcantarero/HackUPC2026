import * as THREE from "three";
import { WORLD_SCALE } from "../../scene/buildSceneGeometry";

interface GapBoxProps {
  width: number;
  gap: number;
  depth: number;
  height: number;
}

export default function GapBox({ width, gap, depth, height }: GapBoxProps) {
  const gapDepth = gap * WORLD_SCALE;
  return (
    <mesh position={[width / 2, height / 2, -depth - gapDepth / 2]}>
      <boxGeometry args={[width, height, gapDepth]} />
      <meshStandardMaterial
        color="#ff0000"
        transparent
        opacity={0.25}
        side={THREE.DoubleSide}
      />
    </mesh>
  );
}
