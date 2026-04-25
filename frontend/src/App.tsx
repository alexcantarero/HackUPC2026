import "./App.css";
import { Canvas } from "@react-three/fiber";
import { CameraControls } from "@react-three/drei";
import type { FormEvent } from "react";
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
import SolverLoader from "./components/Loader/SolverLoader";

// --- WELCOME WAREHOUSE DATA ---
const WELCOME_WAREHOUSE = `0,0
15000,0
15000,15000
0,15000`;

const WELCOME_OBSTACLES = `7000,7000,1000,1000`;

const WELCOME_CEILING = `0,5000
15000,5000`;

const WELCOME_BAYS = `0, 1500, 1000, 3000, 500, 8, 2000
1, 2500, 1200, 4500, 800, 12, 3500`;

const WELCOME_LAYOUT = `0, 2000, 2000, 0
0, 2000, 4500, 0
0, 2000, 7000, 0
0, 2000, 9500, 0
0, 2000, 12000, 0
1, 10000, 2000, 180
1, 10000, 5500, 180
1, 10000, 9000, 180
1, 10000, 12500, 180
0, 6000, 2000, 90
0, 8500, 2000, 90`;

// --- PARSING FUNCTIONS ---
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
  debug?: string[];
  algorithmResults?: Array<{
    algorithm_name: string;
    score: number;
    training_score: number;
    time_took_to_find_best_sol: number;
    number_of_bays: number;
    outputFile?: string;
    outputCsv?: string;
  }>;
};

type MockComparisonResult = {
  algorithm: string;
  title: string;
  status: string;
  bestScore: string;
  rawScore?: number;
  runtimeMs: string;
  outputFile: string;
  note: string;
  outputCsv?: string | null;
};

// --- MAIN COMPONENT ---
export default function App() {
  const lightRef = useRef<THREE.DirectionalLight>(null);
  const meshRef = useRef<THREE.Mesh>(null);
  const cameraControlsRef = useRef<CameraControls>(null);
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
  const [activeLayoutCsv, setActiveLayoutCsv] = useState<string | null>(null);
  const [selectedAlgoName, setSelectedAlgoName] = useState<string | null>(null);

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
      setActiveLayoutCsv(null);
      setSelectedAlgoName(null);
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
        console.log("[App] API Response:", payload);

        if (!response.ok || !payload.ok) {
          setSolverError(payload.message ?? "Solver request failed");
          setSolverResult(null);
          return;
        }

        if (payload.algorithmResults && payload.algorithmResults.length > 0) {
          const results = payload.algorithmResults.map((ar) => ({
            algorithm: ar.algorithm_name,
            title: ar.algorithm_name.replace("_", " ").toUpperCase(),
            status: "success",
            bestScore: ar.score.toLocaleString(),
            rawScore: ar.score,
            runtimeMs: `${Math.round(ar.time_took_to_find_best_sol * 1000)} ms`,
            outputFile: ar.outputFile || "N/A",
            note: `Placed ${ar.number_of_bays} bays with training score ${ar.training_score.toFixed(2)}`,
            outputCsv: ar.outputCsv,
          }));
          
          results.sort((a, b) => (a.rawScore || 0) - (b.rawScore || 0));
          setComparisonResults(results);
        }

        setSolverResult(payload);
        if (payload.outputCsv) {
          setActiveLayoutCsv(payload.outputCsv);
        }
      } catch {
        setSolverError("Unable to reach solver API.");
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
      warehouse: solverResult?.csvInputs?.warehouse ?? WELCOME_WAREHOUSE,
      ceiling: solverResult?.csvInputs?.ceiling ?? WELCOME_CEILING,
      obstacles: solverResult?.csvInputs?.obstacles ?? WELCOME_OBSTACLES,
      types_of_bays: solverResult?.csvInputs?.types_of_bays ?? WELCOME_BAYS,
      expected_output: activeLayoutCsv ?? solverResult?.outputCsv ?? WELCOME_LAYOUT,
    };
  }, [solverResult, activeLayoutCsv]);

  const sceneGeometry = useMemo(
    () =>
      buildSceneGeometry({
        warehouseOutlineCsv: activeCsvInputs.warehouse,
        ceilingCsv: activeCsvInputs.ceiling,
        obstaclesCsv: activeCsvInputs.obstacles,
      }),
    [activeCsvInputs],
  );

  const warehouseCenter = useMemo(() => {
    const points = parseWarehouseOutlineCsv(activeCsvInputs.warehouse);
    if (points.length === 0) return { centerX: 0, centerY: 0 };

    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
    for (const [x, y] of points) {
      if (x < minX) minX = x; if (x > maxX) maxX = x;
      if (y < minY) minY = y; if (y > maxY) maxY = y;
    }
    return {
      centerX: ((minX + maxX) / 2) * WORLD_SCALE,
      centerY: ((minY + maxY) / 2) * WORLD_SCALE,
    };
  }, [activeCsvInputs.warehouse]);

  const bayData = useMemo(() => parseBaysCsv(activeCsvInputs.types_of_bays), [activeCsvInputs.types_of_bays]);
  const layoutData = useMemo(() => parseLayoutCsv(activeCsvInputs.expected_output), [activeCsvInputs.expected_output]);

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
      {isSubmittingSolver && <SolverLoader />}
      <Topbar
        onToggleCameraView={toggleCameraView}
        onToggleGaps={toggleGaps}
        onOpenSolverPanel={toggleSolverPanel}
        showGaps={showGaps}
      />
      {showSolverPanel && (
        <div className="solver-panel" role="dialog" aria-label="Solver upload">
          <div className="solver-panel-header">
            <h2>Solver</h2>
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
          {solverError && <p className="solver-error">{solverError}</p>}
        </div>
      )}

      {comparisonResults.length > 0 && (
        <div
          className="solver-results-panel"
          role="region"
          aria-label="Algorithm results"
        >
          <div className="solver-panel-header">
            <h2>Algorithm Results</h2>
            <button 
              className="solver-close" 
              onClick={() => setComparisonResults([])}
              title="Clear results"
            >
              ×
            </button>
          </div>
          <div
            className="solver-comparison"
            aria-label="Algorithm run summaries"
          >
            {comparisonResults.map((result, idx) => (
              <article
                key={`${result.algorithm}-${idx}`}
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
                <button
                  type="button"
                  className={`solver-view-layout-btn ${selectedAlgoName === result.algorithm ? 'active' : ''}`}
                  onClick={() => {
                    if (result.outputCsv) {
                      setActiveLayoutCsv(result.outputCsv);
                      setSelectedAlgoName(result.algorithm);
                    }
                  }}
                >
                  {selectedAlgoName === result.algorithm ? 'Viewing Layout' : 'View Layout'}
                </button>
              </article>
            ))}
          </div>
        </div>
      )}
      <div className="canvas-container">
        <Canvas
          className="canvas"
          camera={{
            position: cameraPosition,
            fov: 50,
          }}
        >
          <Suspense fallback={null}>
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
            />

            <CameraControls ref={cameraControlsRef} makeDefault />
          </Suspense>
        </Canvas>
      </div>
    </div>
  );
}
