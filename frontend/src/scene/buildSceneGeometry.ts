import * as THREE from "three";

export const WORLD_SCALE = 0.004;
export const FLOOR_THICKNESS = 0.25;
export const CEILING_THICKNESS = 0.2;
export const FLOOR_Y = -2;

export interface SceneGeometryInput {
  warehouseOutlineCsv: string;
  ceilingCsv: string;
  obstaclesCsv: string;
}

export interface SceneObstacle {
  geometry: THREE.BoxGeometry;
  position: [number, number, number];
}

export interface CeilingWallData {
  geometry: THREE.BufferGeometry;
  topY: number;
  normal: [number, number, number];
  center: [number, number, number];
}

export interface SceneGeometry {
  floor: THREE.ExtrudeGeometry;
  ceilingParts: Array<{ geometry: THREE.BufferGeometry; y: number }>;
  ceilingWalls: CeilingWallData[];
  walls: THREE.BufferGeometry[];
  obstacles: SceneObstacle[];
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
    start.x, bottomY, -start.y,
    end.x,   bottomY, -end.y,
    end.x,   topY,    -end.y,
    start.x, bottomY, -start.y,
    end.x,   topY,    -end.y,
    start.x, topY,    -start.y,
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

function parseObstacles(input: string): Array<{ x: number; y: number; w: number; d: number }> {
  return input
    .split("\n")
    .map((line) => line.trim())
    .filter(Boolean)
    .map((line) => {
      const [x, y, w, d] = line.split(",").map((value) => Number(value.trim()));
      return { x, y, w, d };
    })
    .filter(({ x, y, w, d }) => [x, y, w, d].every(Number.isFinite));
}

export function buildSceneGeometry({
  warehouseOutlineCsv,
  ceilingCsv,
  obstaclesCsv,
}: SceneGeometryInput): SceneGeometry {
  const rawPoints = parseOutline(warehouseOutlineCsv);
  const rawCeilingProfile = parseOutline(ceilingCsv).sort((a, b) => a[0] - b[0]);

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

  const floorShape = new THREE.Shape(centeredPoints);
  const floor = new THREE.ExtrudeGeometry(floorShape, {
    depth: FLOOR_THICKNESS,
    bevelEnabled: false,
  });

  floor.rotateX(-Math.PI / 2);
  floor.translate(0, -FLOOR_THICKNESS / 2, 0);

  const ceilingProfile =
    rawCeilingProfile.length > 0
      ? rawCeilingProfile
      : ([[0, 3000]] as Array<[number, number]>);

  const maxRawX = Math.max(...points.map(([x]) => x));
  const minRawX = Math.min(...points.map(([x]) => x));

  const ceilingParts: Array<{ geometry: THREE.BufferGeometry; y: number }> = [];
  const ceilingWalls: CeilingWallData[] = [];
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

    const endRawX = i < ceilingProfile.length - 1 ? ceilingProfile[i + 1][0] : maxRawX;
    if (endRawX <= startRawX) continue;

    const startX = startRawX * WORLD_SCALE - centerX;
    const endX = endRawX * WORLD_SCALE - centerX;
    const clippedPolygon = clipBetweenX(centeredPoints, startX, endX);
    if (clippedPolygon.length < 3) continue;

    const ceilingPartGeometry = new THREE.ShapeGeometry(new THREE.Shape(clippedPolygon));
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

          if (Math.abs(start.x - endX) < 1e-5 && Math.abs(end.x - endX) < 1e-5) {
            const bottomY = Math.min(currentHeightY, nextHeightY);
            const topY = Math.max(currentHeightY, nextHeightY);
            const steppingUp = currentHeightY < nextHeightY;
            const wallStart = steppingUp ? end : start;
            const wallEnd = steppingUp ? start : end;

            // Compute center point of the wall
            const centerX3D = (wallStart.x + wallEnd.x) / 2;
            const centerY3D = (bottomY + topY) / 2;
            const centerZ3D = -(wallStart.y + wallEnd.y) / 2;

            // Compute the wall's normal (perpendicular vector facing outward)
            const dx = wallEnd.x - wallStart.x;
            const dz = (-wallEnd.y) - (-wallStart.y);
            const nx = -dz;
            const nz = dx;
            const length = Math.sqrt(nx * nx + nz * nz);

            ceilingWalls.push({
              geometry: buildWallGeometry(wallStart, wallEnd, bottomY, topY),
              topY,
              normal: [nx / length, 0, nz / length],
              center: [centerX3D, centerY3D, centerZ3D],
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
        if (topY > existing.topY) existing.topY = topY;
      } else {
        perimeterWalls.set(key, { start: start.clone(), end: end.clone(), topY });
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

  const obstacles = parseObstacles(obstaclesCsv).map(({ x, y, w, d }) => {
    const centerXRaw = x + w / 2;
    const centerYRaw = y + d / 2;

    const topY = getCeilingTopY(centerXRaw);
    const bottomY = FLOOR_THICKNESS / 2;
    const height3D = topY - bottomY;

    const width3D = w * WORLD_SCALE;
    const depth3D = d * WORLD_SCALE;
    const geometry = new THREE.BoxGeometry(width3D, height3D, depth3D);

    const cx = centerXRaw * WORLD_SCALE - centerX;
    const cz = -(centerYRaw * WORLD_SCALE - centerY);
    const cy = bottomY + height3D / 2;

    return { geometry, position: [cx, cy, cz] as [number, number, number] };
  });

  return { floor, ceilingParts, ceilingWalls, walls, obstacles };
}