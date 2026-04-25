import "./App.css";
import { Canvas } from "@react-three/fiber";
import { CameraControls } from "@react-three/drei";
import type { FormEvent } from "react";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import * as THREE from "three";
import FadingDollhouseElement from "./components/FadingDollhouseElement";
import {
  buildSceneGeometry,
  FLOOR_Y,
  WORLD_SCALE,
} from "./scene/buildSceneGeometry";
import Topbar from "./components/Topbar/Topbar";

const DEFAULT_CASE_NUMBER = 3;

type RequiredCsvField = "warehouse" | "obstacles" | "ceiling" | "types_of_bays";

type SolveResponse = {
  ok: boolean;
  message: string;
  stdout: string;
  stderr: string;
  bestScore: number | null;
  outputFileName: string | null;
  outputCsv: string | null;
  csvInputs: Partial<Record<RequiredCsvField, string>>;
  exitCode: number;
  durationMs: number;
};

type AlgorithmKey = "greedy" | "sa" | "genetic";

type MockComparisonResult = {
  algorithm: AlgorithmKey;
  title: string;
  status: string;
  bestScore: string;
  runtimeMs: string;
  outputFile: string;
  note: string;
};

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

function createMockComparisonResults(
  caseNumber: number,
): MockComparisonResult[] {
  const caseOffset = caseNumber * 137;

  return [
    {
      algorithm: "greedy",
      title: "Greedy",
      status: "Finished first",
      bestScore: String(18240 - caseOffset),
      runtimeMs: `${420 + (caseOffset % 80)} ms`,
      outputFile: `solution_case${caseNumber}_greedy.csv`,
      note: "Fast baseline, good for quick comparisons.",
    },
    {
      algorithm: "sa",
      title: "Simulated Annealing",
      status: "Finished second",
      bestScore: String(17980 - caseOffset),
      runtimeMs: `${1180 + (caseOffset % 140)} ms`,
      outputFile: `solution_case${caseNumber}_sa.csv`,
      note: "Balances speed and exploration.",
    },
    {
      algorithm: "genetic",
      title: "Genetic",
      status: "Finished last",
      bestScore: String(17650 - caseOffset),
      runtimeMs: `${2680 + (caseOffset % 220)} ms`,
      outputFile: `solution_case${caseNumber}_genetic.csv`,
      note: "Best mock score in this preview.",
    },
  ];
}

function GapBox({
  width,
  gap,
  depth,
  height,
}: {
  width: number;
  gap: number;
  depth: number;
  height: number;
}) {
  const gapDepth = gap * WORLD_SCALE;
  // Position it in FRONT of the bay.
  // The group is at [anchorX, FLOOR_Y, anchorZ].
  // ProceduralShelf is centered at [width/2, ..., -depth/2].
  // So "Front" is at Z = -depth. The gap should extend from -depth to -depth - gapDepth.
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

function ProceduralShelf({
  width,
  height,
  depth,
  color = "#3a82f7",
}: {
  width: number;
  height: number;
  depth: number;
  color?: string;
}) {
  const pillarSize = 0.15;
  const shelfThickness = 0.1;
  const numLevels = 4; // This creates exactly 3 gaps
  const zFightingOffset = 0.01;
  const boundaryInset = 0.01; // Inset pillars to let floor/walls win the sides
  const pillarHeight = height - 0.05; // Slightly shorter to let top shelf win

  return (
    <group position={[width / 2, zFightingOffset, -depth / 2]}>
      {/* 4 Vertical Pillars */}
      {/* Front Left */}
      <mesh
        position={[
          -width / 2 + pillarSize / 2 + boundaryInset,
          pillarHeight / 2,
          depth / 2 - pillarSize / 2 - boundaryInset,
        ]}
      >
        <boxGeometry args={[pillarSize, pillarHeight, pillarSize]} />
        <meshStandardMaterial color="#333333" />
      </mesh>
      {/* Front Right */}
      <mesh
        position={[
          width / 2 - pillarSize / 2 - boundaryInset,
          pillarHeight / 2,
          depth / 2 - pillarSize / 2 - boundaryInset,
        ]}
      >
        <boxGeometry args={[pillarSize, pillarHeight, pillarSize]} />
        <meshStandardMaterial color="#333333" />
      </mesh>
      {/* Back Left */}
      <mesh
        position={[
          -width / 2 + pillarSize / 2 + boundaryInset,
          pillarHeight / 2,
          -depth / 2 + pillarSize / 2 + boundaryInset,
        ]}
      >
        <boxGeometry args={[pillarSize, pillarHeight, pillarSize]} />
        <meshStandardMaterial color="#333333" />
      </mesh>
      {/* Back Right */}
      <mesh
        position={[
          width / 2 - pillarSize / 2 - boundaryInset,
          pillarHeight / 2,
          -depth / 2 + pillarSize / 2 + boundaryInset,
        ]}
      >
        <boxGeometry args={[pillarSize, pillarHeight, pillarSize]} />
        <meshStandardMaterial color="#333333" />
      </mesh>

      {/* Horizontal Shelves */}
      {Array.from({ length: numLevels }).map((_, i) => {
        const bottomGap = 1;
        const availableHeight = height - shelfThickness - bottomGap;
        const yPos = bottomGap + (i / (numLevels - 1)) * availableHeight;
        const isTop = i === numLevels - 1;

        return (
          <mesh key={i} position={[0, yPos + shelfThickness / 2, 0]}>
            {/* Top shelf extends slightly past the pillars to win Z-fighting on both top and side faces */}
            <boxGeometry
              args={[
                isTop
                  ? width - boundaryInset * 2 + 0.02
                  : width - (boundaryInset + pillarSize) * 0.5,
                shelfThickness,
                isTop
                  ? depth - boundaryInset * 2 + 0.02
                  : depth - (boundaryInset + pillarSize) * 0.5,
              ]}
            />
            <meshStandardMaterial color={color} />
          </mesh>
        );
      })}
    </group>
  );
}

// --- MAIN COMPONENT ---
export default function App() {
  const lightRef = useRef<THREE.DirectionalLight>(null);
  const meshRef = useRef<THREE.Mesh>(null);
  const cameraControlsRef = useRef<CameraControls>(null);
  const [caseNumber, setCaseNumber] = useState(DEFAULT_CASE_NUMBER);
  const [cameraPosition] = useState<[number, number, number]>([0, 5, 200]);
  const [, setIsTopView] = useState(false);
  const [showGaps, setShowGaps] = useState(false);
  const [showSolverPanel, setShowSolverPanel] = useState(false);
  const [uploadFiles, setUploadFiles] = useState<
    Partial<Record<RequiredCsvField, File>>
  >({});
  const [isSubmittingSolver, setIsSubmittingSolver] = useState(false);
  const [solverError, setSolverError] = useState("");
  const [solverResult, setSolverResult] = useState<SolveResponse | null>(null);
  const [comparisonResults, setComparisonResults] = useState<
    MockComparisonResult[]
  >([]);

  const toggleGaps = useCallback(() => {
    setShowGaps((prev) => !prev);
  }, []);

  const setUploadFile = useCallback(
    (field: RequiredCsvField, file: File | null) => {
      setUploadFiles((prev) => {
        if (!file) {
          const next = { ...prev };
          delete next[field];
          return next;
        }
        return { ...prev, [field]: file };
      });
    },
    [],
  );

  const toggleSolverPanel = useCallback(() => {
    if (isSubmittingSolver) return;
    setShowSolverPanel((prev) => !prev);
  }, [isSubmittingSolver]);

  const closeSolverPanel = useCallback(() => {
    if (isSubmittingSolver) return;
    setShowSolverPanel(false);
  }, [isSubmittingSolver]);

  const runMockComparison = useCallback(() => {
    setComparisonResults(createMockComparisonResults(caseNumber));
  }, [caseNumber]);

  const submitSolverRequest = useCallback(
    async (event: FormEvent<HTMLFormElement>) => {
      event.preventDefault();

      const requiredFields: RequiredCsvField[] = [
        "warehouse",
        "obstacles",
        "ceiling",
        "types_of_bays",
      ];

      for (const field of requiredFields) {
        if (!uploadFiles[field]) {
          setSolverError(`Missing file: ${field}.csv`);
          return;
        }
      }

      setSolverError("");
      setSolverResult(null);
      setIsSubmittingSolver(true);

      const formData = new FormData();
      for (const field of requiredFields) {
        formData.append(field, uploadFiles[field] as File);
      }
      formData.append("includeOutputCsv", "true");
      formData.append("mode", "multiple");

      try {
        const apiBase =
          import.meta.env.VITE_SOLVER_API_URL ?? "http://localhost:8787";
        const response = await fetch(`${apiBase}/api/solve`, {
          method: "POST",
          body: formData,
        });

        const payload = (await response.json()) as SolveResponse & {
          message?: string;
        };

        if (!response.ok || !payload.ok) {
          setSolverError(payload.message ?? "Solver request failed");
          setSolverResult(null);
          return;
        }

        setSolverResult(payload);
      } catch {
        setSolverError(
          "Unable to reach solver API. Start it with: npm run api",
        );
      } finally {
        setIsSubmittingSolver(false);
      }
    },
    [uploadFiles],
  );

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

  const activeCsvInputs = useMemo(() => {
    return {
      warehouse:
        solverResult?.csvInputs?.warehouse ??
        getCaseCsv(warehouseFiles, caseNumber, "warehouse"),
      ceiling:
        solverResult?.csvInputs?.ceiling ??
        getCaseCsv(ceilingFiles, caseNumber, "ceiling"),
      obstacles:
        solverResult?.csvInputs?.obstacles ??
        getCaseCsv(obstacleFiles, caseNumber, "obstacles"),
      types_of_bays:
        solverResult?.csvInputs?.types_of_bays ??
        getCaseCsv(bayFiles, caseNumber, "types_of_bays"),
      expected_output:
        solverResult?.outputCsv ??
        getCaseCsv(layoutFiles, caseNumber, "expected_output"),
    };
  }, [caseNumber, solverResult]);

  const sceneGeometry = useMemo(
    () =>
      buildSceneGeometry({
        warehouseOutlineCsv: activeCsvInputs.warehouse,
        ceilingCsv: activeCsvInputs.ceiling,
        obstaclesCsv: activeCsvInputs.obstacles,
      }),
    [activeCsvInputs],
  );

  // Re-added: Calculate center of the warehouse
  const warehouseCenter = useMemo(() => {
    const outlineCsv = activeCsvInputs.warehouse;
    const points = parseWarehouseOutlineCsv(outlineCsv);

    if (points.length === 0) {
      return { centerX: 0, centerY: 0 };
    }

    let minX = Infinity;
    let maxX = -Infinity;
    let minY = Infinity;
    let maxY = -Infinity;

    for (const [x, y] of points) {
      if (x < minX) minX = x;
      if (x > maxX) maxX = x;
      if (y < minY) minY = y;
      if (y > maxY) maxY = y;
    }

    return {
      centerX: ((minX + maxX) / 2) * WORLD_SCALE,
      centerY: ((minY + maxY) / 2) * WORLD_SCALE,
    };
  }, [activeCsvInputs.warehouse]);

  // Re-added: Parse bay dimensions
  const bayData = useMemo(() => {
    const csv = activeCsvInputs.types_of_bays;
    return parseBaysCsv(csv);
  }, [activeCsvInputs.types_of_bays]);

  // Re-added: Parse expected output locations
  const layoutData = useMemo(() => {
    const csv = activeCsvInputs.expected_output;
    return parseLayoutCsv(csv);
  }, [activeCsvInputs.expected_output]);

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
        onOpenSolverPanel={toggleSolverPanel}
        showGaps={showGaps}
      />
      {showSolverPanel && (
        <>
          <div
            className="solver-panel"
            role="dialog"
            aria-label="Solver upload"
          >
            <div className="solver-panel-header">
              <h2>Solver</h2>
              <button
                type="button"
                className="solver-close"
                onClick={closeSolverPanel}
                disabled={isSubmittingSolver}
              >
                Close
              </button>
            </div>
            <form className="solver-form" onSubmit={submitSolverRequest}>
              <label>
                warehouse.csv
                <input
                  type="file"
                  accept=".csv,text/csv"
                  onChange={(event) =>
                    setUploadFile("warehouse", event.target.files?.[0] ?? null)
                  }
                />
              </label>
              <label>
                obstacles.csv
                <input
                  type="file"
                  accept=".csv,text/csv"
                  onChange={(event) =>
                    setUploadFile("obstacles", event.target.files?.[0] ?? null)
                  }
                />
              </label>
              <label>
                ceiling.csv
                <input
                  type="file"
                  accept=".csv,text/csv"
                  onChange={(event) =>
                    setUploadFile("ceiling", event.target.files?.[0] ?? null)
                  }
                />
              </label>
              <label>
                types_of_bays.csv
                <input
                  type="file"
                  accept=".csv,text/csv"
                  onChange={(event) =>
                    setUploadFile(
                      "types_of_bays",
                      event.target.files?.[0] ?? null,
                    )
                  }
                />
              </label>
              <button type="submit" disabled={isSubmittingSolver}>
                {isSubmittingSolver ? "Running solver..." : "Upload and run"}
              </button>
            </form>
            <div className="solver-comparison-actions">
              <button
                type="button"
                className="solver-secondary"
                onClick={runMockComparison}
              >
                Mock compare greedy / sa / genetic
              </button>
              <p>
                This is a mock preview of a parallel run summary. Results are
                shown in the left panel.
              </p>
            </div>
            {solverError && <p className="solver-error">{solverError}</p>}
            {solverResult && (
              <div className="solver-result">
                <p>Status: {solverResult.message}</p>
                <p>Best score: {solverResult.bestScore ?? "N/A"}</p>
                <p>Exit code: {solverResult.exitCode}</p>
                <p>Runtime: {Math.round(solverResult.durationMs)} ms</p>
                {solverResult.outputFileName && (
                  <p>Output file: {solverResult.outputFileName}</p>
                )}
                <details>
                  <summary>stdout</summary>
                  <pre>{solverResult.stdout}</pre>
                </details>
                {solverResult.stderr && (
                  <details>
                    <summary>stderr</summary>
                    <pre>{solverResult.stderr}</pre>
                  </details>
                )}
                {solverResult.outputCsv && (
                  <details>
                    <summary>output csv</summary>
                    <pre>{solverResult.outputCsv}</pre>
                  </details>
                )}
              </div>
            )}
          </div>

          <div
            className="solver-results-panel"
            role="region"
            aria-label="Algorithm results"
          >
            <div className="solver-panel-header">
              <h2>Algorithm Results</h2>
            </div>
            <div
              className="solver-comparison"
              aria-label="Mock comparison results"
            >
              {comparisonResults.length === 0 ? (
                <div className="solver-comparison-empty">
                  No comparison results yet. Run the mock comparison from the
                  right panel.
                </div>
              ) : (
                comparisonResults.map((result) => (
                  <article
                    key={result.algorithm}
                    className="solver-comparison-card"
                  >
                    <div className="solver-comparison-card-header">
                      <div>
                        <span className="solver-comparison-badge">
                          {result.algorithm}
                        </span>
                        <h3>{result.title}</h3>
                      </div>
                      <span className="solver-comparison-status">
                        {result.status}
                      </span>
                    </div>
                    <div className="solver-comparison-metrics">
                      <div>
                        <span>Best score</span>
                        <strong>{result.bestScore}</strong>
                      </div>
                      <div>
                        <span>Runtime</span>
                        <strong>{result.runtimeMs}</strong>
                      </div>
                      <div>
                        <span>Output</span>
                        <strong>{result.outputFile}</strong>
                      </div>
                    </div>
                    <p className="solver-comparison-note">{result.note}</p>
                  </article>
                ))
              )}
            </div>
          </div>
        </>
      )}
      <div className="canvas-container">
        <Canvas
          className="canvas"
          camera={{
            position: cameraPosition,
            fov: 50,
          }}
        >
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
          {layoutData &&
            layoutData.map((item, index) => {
              const bay = bayData[item.id];
              if (!bay) return null; // Fallback if bay type is missing

              // 1. Calculate actual world dimensions
              const boxWidth = bay.width * WORLD_SCALE;
              const boxHeight = bay.height * WORLD_SCALE;
              const boxDepth = bay.depth * WORLD_SCALE;

              // 2. Base layout coordinates matched to the centered warehouse
              const anchorX = item.x * WORLD_SCALE - warehouseCenter.centerX;
              const anchorZ = -(item.y * WORLD_SCALE - warehouseCenter.centerY);

              // 3. Rotation
              const rotationY = THREE.MathUtils.degToRad(item.rot);

              // 4. Generate color based on ID
              // Shift the hue by 40 degrees and restrict to a 300-degree range
              // to avoid the red spectrum (0-40 and 340-360) used by gaps.
              const hue = 40 + ((item.id * 137.5) % 300);
              const color = `hsl(${hue}, 85%, 45%)`;

              return (
                <group
                  key={`bay-${item.id}-${index}`}
                  position={[anchorX, FLOOR_Y, anchorZ]}
                  rotation={[0, rotationY, 0]}
                >
                  <ProceduralShelf
                    width={boxWidth}
                    height={boxHeight}
                    depth={boxDepth}
                    color={color}
                  />
                  {showGaps && (
                    <GapBox
                      width={boxWidth}
                      gap={bay.gap}
                      depth={boxDepth}
                      height={boxHeight}
                    />
                  )}
                </group>
              );
            })}
          <CameraControls ref={cameraControlsRef} makeDefault />
        </Canvas>
      </div>
    </div>
  );
}
