import "./App.css";
import { Canvas } from "@react-three/fiber";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import * as THREE from "three";
import { CameraControls } from "@react-three/drei";

import { buildSceneGeometry, WORLD_SCALE } from "./scene/buildSceneGeometry";
import Topbar from "./components/Topbar/Topbar";
import WarehouseScene from "./components/Warehouse/WarehouseScene";
import SolverLoader from "./components/Loader/SolverLoader";
import SolverPanel from "./components/Solver/SolverPanel";
import AlgorithmResults from "./components/Solver/AlgorithmResults";

import { useSolver } from "./hooks/useSolver";
import { WELCOME_WAREHOUSE, WELCOME_CEILING, WELCOME_OBSTACLES, WELCOME_BAYS, WELCOME_LAYOUT } from "./constants/welcomeData";
import { parseBaysCsv, parseLayoutCsv, parseWarehouseOutlineCsv } from "./utils/csvParsers";

export default function App() {
  const lightRef = useRef<THREE.DirectionalLight>(null);
  const meshRef = useRef<THREE.Mesh>(null);
  const cameraControlsRef = useRef<CameraControls>(null);

  const [cameraPosition] = useState<[number, number, number]>([0, 5, 200]);
  const [showGaps, setShowGaps] = useState(false);
  const [showSolverPanel, setShowSolverPanel] = useState(false);

  const {
    setUploadFile,
    isSubmitting,
    error,
    result,
    comparisonResults,
    activeLayoutCsv,
    setActiveLayoutCsv,
    selectedAlgoName,
    setSelectedAlgoName,
    submitRequest,
    setComparisonResults,
  } = useSolver();

  const toggleGaps = useCallback(() => setShowGaps((prev) => !prev), []);
  const toggleSolverPanel = useCallback(() => {
    if (isSubmitting) return;
    setShowSolverPanel((prev) => !prev);
  }, [isSubmitting]);

  const setTopView = useCallback(() => {
    cameraControlsRef.current?.setLookAt(0, 150, 0, 0, -2, 0, true);
  }, []);

  const setPerspectiveView = useCallback(() => {
    cameraControlsRef.current?.setLookAt(
      cameraPosition[0],
      cameraPosition[1],
      cameraPosition[2],
      0,
      -2,
      0,
      true,
    );
  }, [cameraPosition]);

  const toggleCameraView = useCallback(() => {
    const camera = cameraControlsRef.current;
    if (!camera) return;

    // Check current camera Y to decide toggle (Top view is at 150)
    const isCurrentlyTop = Math.abs(camera.camera.position.y - 150) < 10;
    
    if (isCurrentlyTop) setPerspectiveView();
    else setTopView();
  }, [setPerspectiveView, setTopView]);

  const activeCsvInputs = useMemo(() => {
    return {
      warehouse: result?.csvInputs?.warehouse ?? WELCOME_WAREHOUSE,
      ceiling: result?.csvInputs?.ceiling ?? WELCOME_CEILING,
      obstacles: result?.csvInputs?.obstacles ?? WELCOME_OBSTACLES,
      types_of_bays: result?.csvInputs?.types_of_bays ?? WELCOME_BAYS,
      expected_output: activeLayoutCsv ?? result?.outputCsv ?? WELCOME_LAYOUT,
    };
  }, [result, activeLayoutCsv]);

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
      -2, // FLOOR_Y is -2
      0,
      false,
    );
  }, [cameraPosition]);

  return (
    <div className="app-shell">
      {isSubmitting && <SolverLoader />}
      
      <Topbar
        onToggleCameraView={toggleCameraView}
        onToggleGaps={toggleGaps}
        onOpenSolverPanel={toggleSolverPanel}
        showGaps={showGaps}
      />

      {showSolverPanel && (
        <SolverPanel
          onClose={toggleSolverPanel}
          onSubmit={submitRequest}
          onFileChange={setUploadFile}
          isSubmitting={isSubmitting}
          error={error}
        />
      )}

      <AlgorithmResults
        results={comparisonResults}
        selectedAlgo={selectedAlgoName}
        onViewLayout={(algo) => {
          if (algo.outputCsv) {
            setActiveLayoutCsv(algo.outputCsv);
            setSelectedAlgoName(algo.algorithm);
          }
        }}
        onClear={() => setComparisonResults([])}
      />

      <div className="canvas-container">
        <Canvas className="canvas" camera={{ position: cameraPosition, fov: 50 }}>
          <WarehouseScene
            sceneGeometry={buildSceneGeometry({
              warehouseOutlineCsv: activeCsvInputs.warehouse,
              ceilingCsv: activeCsvInputs.ceiling,
              obstaclesCsv: activeCsvInputs.obstacles,
            })}
            layoutData={layoutData}
            bayData={bayData}
            warehouseCenter={warehouseCenter}
            showGaps={showGaps}
            lightRef={lightRef}
            meshRef={meshRef}
            cameraControlsRef={cameraControlsRef}
          />
        </Canvas>
      </div>
    </div>
  );
}
