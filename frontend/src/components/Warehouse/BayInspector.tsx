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
            x1={size.width - 130} // Centered on the 220px panel
            y1={size.height - 70} // Adjusted for shorter panel
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
        <div className="solver-comparison-card" style={{ border: "none", background: "none", padding: 0 }}>
          <div className="solver-comparison-metrics" style={{ flexDirection: "column", gap: "6px", background: "none", padding: 0, margin: 0 }}>
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline" }}>
              <span style={{ fontSize: "0.6rem" }}>ID</span>
              <strong style={{ fontSize: "0.9rem" }}>#{selectedBay.id}</strong>
            </div>
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline" }}>
              <span style={{ fontSize: "0.6rem" }}>LOADS</span>
              <strong style={{ color: "#ffffff", fontSize: "0.85rem" }}>{selectedBay.nLoads}</strong>
            </div>
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline" }}>
              <span style={{ fontSize: "0.6rem" }}>PRICE</span>
              <strong style={{ color: "#ed8200", fontSize: "0.85rem" }}>{selectedBay.price}€</strong>
            </div>
          </div>
        </div>
      </div>

      <style>{`
        .bay-inspector-panel {
          position: absolute;
          bottom: 24px;
          right: 24px;
          top: auto;
          width: 180px;
          padding: 12px 16px;
          animation: panelSlideUp 0.15s ease-out;
        }
        @keyframes panelSlideUp {
          from { opacity: 0; transform: scale(0.95); }
          to { opacity: 1; transform: scale(1); }
        }
      `}</style>
    </Html>
  );
}
