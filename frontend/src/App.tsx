import "./App.css";
import { Canvas, useFrame } from "@react-three/fiber";
import { CameraControls } from "@react-three/drei";
import { useRef, useEffect, useMemo } from "react";
import * as THREE from "three";
import Shelf from "./models/shelf.jsx";

const CASE_NUMBER = 0;
const warehouseFiles = import.meta.glob(
  "../../data/input/Case*/warehouse.csv",
  { query: "?raw", import: "default", eager: true },
);
const ceilingFiles = import.meta.glob("../../data/input/Case*/ceiling.csv", {
  query: "?raw",
  import: "default",
  eager: true,
});
const obstacleFiles = import.meta.glob("../../data/input/Case*/obstacles.csv", {
  query: "?raw",
  import: "default",
  eager: true,
});

const warehouseOutlineCsv = warehouseFiles[
  `../../data/input/Case${CASE_NUMBER}/warehouse.csv`
] as string;
const ceilingCsv = ceilingFiles[
  `../../data/input/Case${CASE_NUMBER}/ceiling.csv`
] as string;
const obstaclesCsv = obstacleFiles[
  `../../data/input/Case${CASE_NUMBER}/obstacles.csv`
] as string;

const WORLD_SCALE = 0.004;
const FLOOR_THICKNESS = 0.25;
const CEILING_THICKNESS = 0.2;
const FLOOR_Y = -2;

// --- NEW FADING DOLLHOUSE ELEMENT ---
// This handles both the smooth opacity fade AND the Backface culling!
function FadingDollhouseElement({
  geometry,
  positionY,
  thresholdY,
}: {
  geometry: THREE.BufferGeometry;
  positionY: number;
  thresholdY: number;
}) {
  const matRef = useRef<THREE.MeshStandardMaterial>(null);

  useFrame(({ camera }, delta) => {
    if (matRef.current) {
      // Target opacity: 1 (fully visible) if camera is below the roof, 0 if above
      const targetOpacity = camera.position.y < FLOOR_Y + thresholdY ? 1 : 0;

      // Smoothly animate the current opacity towards the target opacity
      matRef.current.opacity = THREE.MathUtils.damp(
        matRef.current.opacity,
        targetOpacity,
        4, // Damp speed (higher = faster fade)
        delta,
      );
    }
  });

  return (
    <mesh position={[0, positionY, 0]} geometry={geometry}>
      <meshStandardMaterial
        ref={matRef}
        color="#d6d6d6"
        side={THREE.BackSide}
        transparent={true} // Mandatory to allow opacity fading
      />
    </mesh>
  );
}

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

function clipAgainstVertical(
  polygon: THREE.Vector2[],
  xCut: number,
  keepRight: boolean,
): THREE.Vector2[] {
  if (polygon.length === 0) return [];

  const output: THREE.Vector2[] = [];
  const inside = (p: THREE.Vector2) => (keepRight ? p.x >= xCut : p.x <= xCut);

  for (let i = 0; i < polygon.length; i += 1) {
    const current = polygon[i];
    const previous = polygon[(i + polygon.length - 1) % polygon.length];
    const currentInside = inside(current);
    const previousInside = inside(previous);

    if (currentInside !== previousInside) {
      const dx = current.x - previous.x;
      const t = Math.abs(dx) < Number.EPSILON ? 0 : (xCut - previous.x) / dx;
      const y = previous.y + (current.y - previous.y) * t;
      output.push(new THREE.Vector2(xCut, y));
    }

    if (currentInside) output.push(current.clone());
  }

  return output;
}

function clipBetweenX(
  polygon: THREE.Vector2[],
  minX: number,
  maxX: number,
): THREE.Vector2[] {
  return clipAgainstVertical(
    clipAgainstVertical(polygon, minX, true),
    maxX,
    false,
  );
}

function edgeKey(a: THREE.Vector2, b: THREE.Vector2): string {
  const ax = a.x.toFixed(5);
  const ay = a.y.toFixed(5);
  const bx = b.x.toFixed(5);
  const by = b.y.toFixed(5);
  return ax < bx || (ax === bx && ay <= by)
    ? `${ax},${ay}|${bx},${by}`
    : `${bx},${by}|${ax},${ay}`;
}

function buildWallGeometry(
  start: THREE.Vector2,
  end: THREE.Vector2,
  bottomY: number,
  topY: number,
): THREE.BufferGeometry {
  const geometry = new THREE.BufferGeometry();

  const vertices = new Float32Array([
    start.x,
    bottomY,
    -start.y,
    end.x,
    bottomY,
    -end.y,
    end.x,
    topY,
    -end.y,
    start.x,
    bottomY,
    -start.y,
    end.x,
    topY,
    -end.y,
    start.x,
    topY,
    -start.y,
  ]);

  geometry.setAttribute("position", new THREE.BufferAttribute(vertices, 3));
  geometry.computeVertexNormals();
  return geometry;
}

function pointOnSegment(
  point: THREE.Vector2,
  segmentStart: THREE.Vector2,
  segmentEnd: THREE.Vector2,
  epsilon = 1e-5,
): boolean {
  const segment = new THREE.Vector2().subVectors(segmentEnd, segmentStart);
  const toPoint = new THREE.Vector2().subVectors(point, segmentStart);
  const cross = segment.x * toPoint.y - segment.y * toPoint.x;
  if (Math.abs(cross) > epsilon) return false;

  const dot = toPoint.dot(segment);
  if (dot < -epsilon) return false;

  const segmentLenSq = segment.lengthSq();
  if (dot - segmentLenSq > epsilon) return false;
  return true;
}

function edgeOnOutline(
  start: THREE.Vector2,
  end: THREE.Vector2,
  outline: THREE.Vector2[],
): boolean {
  for (let i = 0; i < outline.length; i += 1) {
    const a = outline[i];
    const b = outline[(i + 1) % outline.length];
    if (pointOnSegment(start, a, b) && pointOnSegment(end, a, b)) {
      return true;
    }
  }
  return false;
}

export default function App() {
  const lightRef = useRef<THREE.DirectionalLight>(null);
  const meshRef = useRef<THREE.Mesh>(null);

  const sceneGeometry = useMemo(() => {
    const rawPoints = parseOutline(warehouseOutlineCsv);
    const rawCeilingProfile = parseOutline(ceilingCsv).sort(
      (a, b) => a[0] - b[0],
    );

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

    const ceilingProfile =
      rawCeilingProfile.length > 0
        ? rawCeilingProfile
        : ([[0, 3000]] as Array<[number, number]>);

    const maxRawX = Math.max(...points.map(([x]) => x));
    const minRawX = Math.min(...points.map(([x]) => x));

    const ceilingParts: Array<{ geometry: THREE.BufferGeometry; y: number }> =
      [];
    const ceilingWalls: Array<{
      geometry: THREE.BufferGeometry;
      topY: number;
    }> = [];
    const walls: THREE.BufferGeometry[] = [];
    const perimeterWalls = new Map<
      string,
      { start: THREE.Vector2; end: THREE.Vector2; topY: number }
    >();

    for (let i = 0; i < ceilingProfile.length; i += 1) {
      let [startRawX, rawHeight] = ceilingProfile[i];

      if (i === 0 && minRawX < startRawX) {
        startRawX = minRawX;
      }

      const endRawX =
        i < ceilingProfile.length - 1 ? ceilingProfile[i + 1][0] : maxRawX;

      if (endRawX <= startRawX) continue;

      const startX = startRawX * WORLD_SCALE - centerX;
      const endX = endRawX * WORLD_SCALE - centerX;
      const clippedPolygon = clipBetweenX(centeredPoints, startX, endX);
      if (clippedPolygon.length < 3) continue;

      const ceilingPartGeometry = new THREE.ShapeGeometry(
        new THREE.Shape(clippedPolygon),
      );

      ceilingPartGeometry.rotateX(-Math.PI / 2);
      ceilingPartGeometry.translate(0, -CEILING_THICKNESS / 2, 0);

      if (i < ceilingProfile.length - 1) {
        const nextRawHeight = ceilingProfile[i + 1][1];
        const currentHeightY = rawHeight * WORLD_SCALE - CEILING_THICKNESS / 2;
        const nextHeightY = nextRawHeight * WORLD_SCALE - CEILING_THICKNESS / 2;

        if (Math.abs(currentHeightY - nextHeightY) > 1e-5) {
          for (let j = 0; j < clippedPolygon.length; j += 1) {
            const start = clippedPolygon[j];
            const end = clippedPolygon[(j + 1) % clippedPolygon.length];

            if (start.distanceToSquared(end) < 1e-10) continue;

            if (
              Math.abs(start.x - endX) < 1e-5 &&
              Math.abs(end.x - endX) < 1e-5
            ) {
              const bottomY = Math.min(currentHeightY, nextHeightY);
              const topY = Math.max(currentHeightY, nextHeightY);

              const isSteppingUp = currentHeightY < nextHeightY;
              const pt1 = isSteppingUp ? end : start;
              const pt2 = isSteppingUp ? start : end;

              ceilingWalls.push({
                geometry: buildWallGeometry(pt1, pt2, bottomY, topY),
                topY: topY,
              });
            }
          }
        }
      }

      ceilingParts.push({
        geometry: ceilingPartGeometry,
        y: rawHeight * WORLD_SCALE,
      });

      for (let j = 0; j < clippedPolygon.length; j += 1) {
        const start = clippedPolygon[j];
        const end = clippedPolygon[(j + 1) % clippedPolygon.length];
        if (start.distanceToSquared(end) < 1e-10) continue;
        if (!edgeOnOutline(start, end, centeredPoints)) continue;

        const key = edgeKey(start, end);
        const topY = rawHeight * WORLD_SCALE - CEILING_THICKNESS / 2;
        const existing = perimeterWalls.get(key);
        if (existing) {
          if (topY > existing.topY) {
            existing.topY = topY;
          }
        } else {
          perimeterWalls.set(key, {
            start: start.clone(),
            end: end.clone(),
            topY,
          });
        }
      }
    }

    const floorTopY = FLOOR_THICKNESS / 2;

    for (const { start, end, topY } of perimeterWalls.values()) {
      if (topY <= floorTopY + 1e-5) continue;
      walls.push(buildWallGeometry(start, end, floorTopY, topY));
    }

    const getCeilingTopY = (targetXRaw: number) => {
      let h = ceilingProfile[0][1];
      for (const [cx, ch] of ceilingProfile) {
        if (targetXRaw >= cx) h = ch;
      }
      return h * WORLD_SCALE - CEILING_THICKNESS / 2;
    };

    const parsedObstacles = obstaclesCsv
      .split("\n")
      .map((l) => l.trim())
      .filter(Boolean)
      .map((l) => {
        const [x, y, w, d] = l.split(",").map((v) => Number(v.trim()));
        return { x, y, w, d };
      });

    const obstacles: Array<{
      geometry: THREE.BoxGeometry;
      position: [number, number, number];
    }> = [];

    for (const { x, y, w, d } of parsedObstacles) {
      const centerXRaw = x + w / 2;
      const centerYRaw = y + d / 2;

      const topY = getCeilingTopY(centerXRaw);
      const bottomY = FLOOR_THICKNESS / 2;
      const height3D = topY - bottomY;

      const width3D = w * WORLD_SCALE;
      const depth3D = d * WORLD_SCALE;

      const geom = new THREE.BoxGeometry(width3D, height3D, depth3D);

      const cx = centerXRaw * WORLD_SCALE - centerX;
      const cz = -(centerYRaw * WORLD_SCALE - centerY);
      const cy = bottomY + height3D / 2;

      obstacles.push({ geometry: geom, position: [cx, cy, cz] });
    }

    return {
      floor: geometry,
      ceilingParts,
      ceilingWalls,
      walls,
      obstacles,
    };
  }, []);

  useEffect(() => {
    return () => {
      sceneGeometry.floor.dispose();
      for (const part of sceneGeometry.ceilingParts) part.geometry.dispose();
      for (const cw of sceneGeometry.ceilingWalls) cw.geometry.dispose();
      for (const wall of sceneGeometry.walls) wall.dispose();
      for (const obs of sceneGeometry.obstacles) obs.geometry.dispose();
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
      <Canvas className="canvas" camera={{ position: [0, 5, 40], fov: 50 }}>
        <color attach="background" args={["#bbbbbb"]} />
        <directionalLight ref={lightRef} position={[0, 5, 5]} intensity={5} />
        <ambientLight intensity={3} />

        <mesh
          position={[0, FLOOR_Y, 0]}
          ref={meshRef}
          geometry={sceneGeometry.floor}
        >
          <meshStandardMaterial color="#a3a3a3" />
        </mesh>

        {/* --- Render main flat ceiling roofs with the fade --- */}
        {sceneGeometry.ceilingParts.map((part, index) => (
          <FadingDollhouseElement
            key={`ceiling-${index}`}
            geometry={part.geometry}
            positionY={FLOOR_Y + part.y}
            thresholdY={part.y}
          />
        ))}

        {/* --- Render the vertical ceiling steps with the fade --- */}
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

        {sceneGeometry.obstacles.map((obs, index) => (
          <mesh
            key={`obstacle-${index}`}
            position={[
              obs.position[0],
              FLOOR_Y + obs.position[1],
              obs.position[2],
            ]}
            geometry={obs.geometry}
          >
            <meshStandardMaterial color="#a8a8a8" side={THREE.FrontSide} />
          </mesh>
        ))}

        <Shelf position={[0, FLOOR_Y, 0]} scale={0.05} />
        <CameraControls makeDefault />
      </Canvas>
    </div>
  );
}
