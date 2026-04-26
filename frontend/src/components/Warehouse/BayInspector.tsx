import { useState } from "react";
import * as THREE from "three";
import { useThree, useFrame } from "@react-three/fiber";
import { Html } from "@react-three/drei";

interface BayInfo {
  id: number;
  nLoads: number;
  price: number;
  position: [number, number, number];
}

interface BayInspectorProps {
  selectedBay: BayInfo | null;
}

export default function BayInspector({ selectedBay }: BayInspectorProps) {
  const { camera, size } = useThree();
  const [screenPos, setScreenPos] = useState({ x: 0, y: 0 });

  useFrame(() => {
    if (!selectedBay) return;

    const vector = new THREE.Vector3(...selectedBay.position);
    vector.project(camera);

    const x = (vector.x * 0.5 + 0.5) * size.width;
    const y = (vector.y * -0.5 + 0.5) * size.height;

    setScreenPos({ x, y });
  });

  if (!selectedBay) return null;

  return (
    <Html fullscreen style={{ pointerEvents: "none" }}>
      {/* The Arrow (SVG Overlay) */}
      <div
        style={{
          position: "absolute",
          top: 0,
          left: 0,
          width: "100%",
          height: "100%",
        }}
      >
        <svg width="100%" height="100%">
          <defs>
            <marker
              id="arrowhead"
              markerWidth="10"
              markerHeight="7"
              refX="0"
              refY="3.5"
              orient="auto"
            >
              <polygon points="0 0, 10 3.5, 0 7" fill="#3a82f7" />
            </marker>
          </defs>
          <line
            x1={size.width - 164} // Approx center of the bottom-right panel
            y1={size.height - 110}
            x2={screenPos.x}
            y2={screenPos.y}
            stroke="#3a82f7"
            strokeWidth="2"
            strokeDasharray="5,5"
            markerEnd="url(#arrowhead)"
          />
        </svg>
      </div>

      {/* The Panel (Screen Space) */}
      <div className="solver-panel bay-inspector-panel" style={{ pointerEvents: "auto" }}>
        <div className="solver-panel-header">
          <h2>Bay Details</h2>
        </div>
        <div className="solver-comparison-card" style={{ border: "none", background: "none", padding: 0 }}>
          <div className="solver-comparison-metrics" style={{ flexDirection: "column", gap: "12px", background: "none", padding: 0 }}>
            <div style={{ display: "flex", justifyContent: "space-between" }}>
              <span>Bay ID</span>
              <strong style={{ fontSize: "1.2rem" }}>#{selectedBay.id}</strong>
            </div>
            <div style={{ display: "flex", justifyContent: "space-between" }}>
              <span>Loads</span>
              <strong style={{ color: "#ffffff" }}>{selectedBay.nLoads} units</strong>
            </div>
            <div style={{ display: "flex", justifyContent: "space-between" }}>
              <span>Price</span>
              <strong style={{ color: "#ed8200" }}>{selectedBay.price.toLocaleString()} €</strong>
            </div>
          </div>
          <p className="solver-comparison-note" style={{ marginTop: "16px", fontStyle: "italic" }}>
            Operational parameters for this rack configuration.
          </p>
        </div>
      </div>

      <style>{`
        .bay-inspector-panel {
          position: absolute;
          bottom: 24px;
          right: 24px;
          top: auto;
          width: 280px;
          padding: 20px;
          animation: panelSlideUp 0.3s ease-out;
        }
        @keyframes panelSlideUp {
          from { opacity: 0; transform: translateY(20px); }
          to { opacity: 1; transform: translateY(0); }
        }
      `}</style>
    </Html>
  );
}
