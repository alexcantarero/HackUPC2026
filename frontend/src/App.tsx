import "./App.css";
import { Canvas } from "@react-three/fiber";
import { CameraControls } from "@react-three/drei";
import { XR, createXRStore } from "@react-three/xr";
import { useCallback, useEffect, useMemo, useRef, useState, Suspense } from "react";
import * as THREE from "three";
import FadingDollhouseElement from "./components/FadingDollhouseElement";
import {
  buildSceneGeometry,
  FLOOR_Y,
  WORLD_SCALE,
} from "./scene/buildSceneGeometry";
import Topbar from "./components/Topbar/Topbar";
import Warehouse from "./components/Warehouse/Warehouse";
import ARQRDialog from "./components/AR/ARQRDialog";

const DEFAULT_CASE_NUMBER = 3;

// --- FILE LOADERS ---
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

const bayFiles = import.meta.glob("../../data/input/Case*/types_of_bays.csv", {
  query: "?raw",
  import: "default",
  eager: true,
}) as Record<string, string>;

const layoutFiles = import.meta.glob(
  "../../data/input/Case*/expected_output.csv",
  {
    query: "?raw",
    import: "default",
    eager: true,
  },
) as Record<string, string>;

// --- PARSING FUNCTIONS ---
function getCaseCsv(
  files: Record<string, string>,
  caseNumber: number,
  kind: string,
) {
  const filePath = `../../data/input/Case${caseNumber}/${kind}.csv`;
  return files[filePath] ?? "";
}

function parseBaysCsv(csvString: string) {
  if (!csvString) return {};
  const lines = csvString.trim().split("\n");
  const data: Record<
    number,
    { width: number; depth: number; height: number; gap: number }
  > = {};

  for (let i = 0; i < lines.length; i++) {
    const cols = lines[i].split(",").map((s) => Number(s.trim()));
    if (
      cols.length >= 5 &&
      cols.slice(0, 5).every((value) => Number.isFinite(value))
    ) {
      data[cols[0]] = {
        width: cols[1],
        depth: cols[2],
        height: cols[3],
        gap: cols[4],
      };
    }
  }
  return data;
}

function parseLayoutCsv(csvString: string) {
  if (!csvString) return [];
  const lines = csvString.trim().split("\n");
  const data: Array<{ id: number; x: number; y: number; rot: number }> = [];

  for (let i = 0; i < lines.length; i++) {
    const cols = lines[i].split(",").map((s) => Number(s.trim()));
    if (
      cols.length >= 4 &&
      cols.slice(0, 4).every((value) => Number.isFinite(value))
    ) {
      data.push({ id: cols[0], x: cols[1], y: cols[2], rot: cols[3] });
    }
  }
  return data;
}

function parseWarehouseOutlineCsv(csvString: string): Array<[number, number]> {
  if (!csvString) return [];
  return csvString
    .split("\n")
    .map((line) => line.trim())
    .filter(Boolean)
    .map((line) => {
      const [xRaw, yRaw] = line.split(",").map((value) => Number(value.trim()));
      return [xRaw, yRaw] as [number, number];
    })
    .filter(([x, y]) => Number.isFinite(x) && Number.isFinite(y));
}

// --- MAIN COMPONENT ---
export default function App() {
  const [isReady, setIsReady] = useState(false);
  const store = useMemo(() => createXRStore({
    depthSensing: true,
    hitTest: true,
  }), []);

  useEffect(() => {
    const timer = setTimeout(() => setIsReady(true), 500);
    return () => clearTimeout(timer);
  }, []);

  const lightRef = useRef<THREE.DirectionalLight>(null);
  const meshRef = useRef<THREE.Mesh>(null);
  const cameraControlsRef = useRef<CameraControls>(null);
  const [caseNumber, setCaseNumber] = useState(DEFAULT_CASE_NUMBER);
  const [cameraPosition] = useState<[number, number, number]>([0, 5, 200]);
  const [, setIsTopView] = useState(false);
  const [showGaps, setShowGaps] = useState(false);
  const [showARDialog, setShowARDialog] = useState(false);

  const toggleGaps = useCallback(() => {
    setShowGaps((prev) => !prev);
  }, []);

  const toggleARDialog = useCallback(() => {
    setShowARDialog((prev) => !prev);
  }, []);

  const setTopView = useCallback(() => {
    cameraControlsRef.current?.setLookAt(
      0,
      FLOOR_Y + 250,
      0,
      0,
      FLOOR_Y,
      0,
      true,
    );
  }, []);

  const setPerspectiveView = useCallback(() => {
    cameraControlsRef.current?.setLookAt(
      cameraPosition[0],
      cameraPosition[1],
      cameraPosition[2],
      0,
      FLOOR_Y,
      0,
      true,
    );
  }, [cameraPosition]);

  const toggleCameraView = useCallback(() => {
    setIsTopView((prevIsTopView) => {
      const nextIsTopView = !prevIsTopView;
      if (nextIsTopView) setTopView();
      else setPerspectiveView();
      return nextIsTopView;
    });
  }, [setPerspectiveView, setTopView]);

  const sceneGeometry = useMemo(
    () =>
      buildSceneGeometry({
        warehouseOutlineCsv: getCaseCsv(
          warehouseFiles,
          caseNumber,
          "warehouse",
        ),
        ceilingCsv: getCaseCsv(ceilingFiles, caseNumber, "ceiling"),
        obstaclesCsv: getCaseCsv(obstacleFiles, caseNumber, "obstacles"),
      }),
    [caseNumber],
  );

  const warehouseCenter = useMemo(() => {
    const outlineCsv = getCaseCsv(warehouseFiles, caseNumber, "warehouse");
    const points = parseWarehouseOutlineCsv(outlineCsv);

    if (points.length === 0) {
      return { centerX: 0, centerY: 0 };
    }

    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
    for (const [x, y] of points) {
      if (x < minX) minX = x; if (x > maxX) maxX = x;
      if (y < minY) minY = y; if (y > maxY) maxY = y;
    }

    return {
      centerX: ((minX + maxX) / 2) * WORLD_SCALE,
      centerY: ((minY + maxY) / 2) * WORLD_SCALE,
    };
  }, [caseNumber]);

  const bayData = useMemo(() => {
    const csv = getCaseCsv(bayFiles, caseNumber, "types_of_bays");
    return parseBaysCsv(csv);
  }, [caseNumber]);

  const layoutData = useMemo(() => {
    const csv = getCaseCsv(layoutFiles, caseNumber, "expected_output");
    return parseLayoutCsv(csv);
  }, [caseNumber]);

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

  useEffect(() => {
    cameraControlsRef.current?.setLookAt(
      cameraPosition[0],
      cameraPosition[1],
      cameraPosition[2],
      0,
      FLOOR_Y,
      0,
      false,
    );
  }, [cameraPosition]);

  return (
    <div className="app-shell">
      <Topbar
        caseNumber={caseNumber}
        onCaseChange={setCaseNumber}
        onToggleCameraView={toggleCameraView}
        onToggleGaps={toggleGaps}
        showGaps={showGaps}
        onToggleAR={toggleARDialog}
        showAR={showARDialog}
      />

      <ARQRDialog open={showARDialog} onClose={() => setShowARDialog(false)} />

      {!isReady ? (
        <div style={{ position: 'fixed', inset: 0, display: 'flex', alignItems: 'center', justifyContent: 'center', backgroundColor: '#878787', color: 'white' }}>
          <h2>Loading Warehouse...</h2>
        </div>
      ) : (
        <>
          <div 
            style={{ 
              position: 'absolute', 
              bottom: 40, 
              left: '50%', 
              transform: 'translateX(-50%)', 
              zIndex: 1000,
              display: ('ontouchstart' in window) ? 'block' : 'none' 
            }}
          >
            <button 
              onClick={async () => {
                try {
                  await store.enterAR();
                } catch (e) {
                  alert(
                    "WebXR is not supported by your current browser.\n\n" +
                    "If you are on an iPhone, standard Safari does not support AR. " +
                    "Please download 'WebXR Viewer' by Mozilla from the App Store and open this link there to view the hologram."
                  );
                }
              }}
              style={{
                padding: '16px 32px',
                backgroundColor: '#ff5722',
                color: 'white',
                border: 'none',
                borderRadius: '50px',
                fontWeight: 'bold',
                fontSize: '1.1rem',
                cursor: 'pointer',
                boxShadow: '0 8px 15px rgba(0,0,0,0.3)',
                WebkitAppearance: 'none'
              }}
            >
              PROJECT ON TABLE (AR)
            </button>
          </div>

          <div className="canvas-container">
            <Canvas
              className="canvas"
              camera={{ position: cameraPosition, fov: 50 }}
            >
              <XR store={store}>
                <Suspense fallback={null}>
                  <color attach="background" args={["#878787"]} />
                  <directionalLight ref={lightRef} position={[0, 5, 5]} intensity={5} />
                  <ambientLight intensity={3} />

                  <mesh position={[0, FLOOR_Y, 0]} ref={meshRef} geometry={sceneGeometry.floor}>
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
                  />

                  <CameraControls ref={cameraControlsRef} makeDefault />
                </Suspense>
              </XR>
            </Canvas>
          </div>
        </>
      )}
    </div>
  );
}
