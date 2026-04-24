import "./App.css";
import { Canvas } from "@react-three/fiber";
import { CameraControls } from "@react-three/drei";
import { useRef, useEffect } from "react";
import * as THREE from "three";
import Shelf from "./models/shelf.jsx";

export default function App() {
  const lightRef = useRef<THREE.DirectionalLight>(null);
  const meshRef = useRef<THREE.Mesh>(null);

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
        <mesh position={[0, -2, 0]} ref={meshRef}>
          <boxGeometry args={[20, 0.25, 20]} />
          <meshStandardMaterial color="#a3a3a3" />
        </mesh>
        <Shelf position={[0, -2, 0]} scale={0.05} />
        <CameraControls makeDefault />
      </Canvas>
    </div>
  );
}
