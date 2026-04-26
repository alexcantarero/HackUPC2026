import { useState, useEffect } from "react";
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
  const [screenPos, setScreenPos] = useState<{ x: number; y: number } | null>(null);

  // Fix flicker: Clear position immediately when the selected bay changes
  useEffect(() => {
    setScreenPos(null);
  }, [selectedBay?.id]);

  useFrame(() => {
    if (!selectedBay) return;

    const vector = new THREE.Vector3(...selectedBay.position);
    vector.project(camera);

    const x = (vector.x * 0.5 + 0.5) * size.width;
    const y = (vector.y * -0.5 + 0.5) * size.height;

    setScreenPos({ x, y });
  });

  if (!selectedBay || !screenPos) return null;

  // Use 16px offset for mobile, 24px for desktop
  const isSmallScreen = size.width < 960;
  const offset = isSmallScreen ? 16 : 24;
  const panelWidth = isSmallScreen ? 170 : 180;
  const panelHeight = isSmallScreen ? 120 : 75; // Symmetrical height

  // Anchor for the arrow should be at the top-center of the panel
  const startX = size.width - offset - (panelWidth / 2);
  const startY = size.height - offset - panelHeight;

  return (
    <Html fullscreen style={{ pointerEvents: "none", zIndex: 50 }}>
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
              refX="10" 
              refY="3.5"
              orient="auto"
            >
              <polygon points="0 0, 10 3.5, 0 7" fill="#3a82f7" />
            </marker>
          </defs>
          
          {/* Conditional Path: Straight on mobile, L-shaped on desktop */}
          {isSmallScreen ? (
            <line
              x1={startX}
              y1={startY}
              x2={screenPos.x}
              y2={screenPos.y}
              stroke="#3a82f7"
              strokeWidth="2"
              strokeDasharray="5,5"
              markerEnd="url(#arrowhead)"
            />
          ) : (
            <path
              d={`M ${startX} ${startY} H ${screenPos.x} V ${screenPos.y}`}
              fill="none"
              stroke="#3a82f7"
              strokeWidth="2"
              strokeDasharray="5,5"
              markerEnd="url(#arrowhead)"
            />
          )}
        </svg>
      </div>

      {/* The Panel (Screen Space) */}
      <div className="solver-panel bay-inspector-panel" style={{ pointerEvents: "auto" }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: isSmallScreen ? '8px' : '0', borderBottom: isSmallScreen ? '1px solid rgba(255,255,255,0.05)' : 'none', paddingBottom: isSmallScreen ? '4px' : '0' }}>
           <span style={{ fontSize: '0.55rem', fontWeight: 800, color: 'var(--text-secondary)', textTransform: 'uppercase' }}>Bay Info</span>
        </div>
        <div className="solver-comparison-card" style={{ border: "none", background: "none", padding: 0 }}>
          <div className="solver-comparison-metrics" style={{ flexDirection: "column", gap: "6px", background: "none", padding: 0, margin: 0 }}>
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline" }}>
              <span style={{ fontSize: isSmallScreen ? "0.4rem" : "0.55rem" }}>ID</span>
              <strong style={{ fontSize: isSmallScreen ? "0.7rem" : "0.85rem" }}>#{selectedBay.id}</strong>
            </div>
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline" }}>
              <span style={{ fontSize: isSmallScreen ? "0.4rem" : "0.55rem" }}>LOADS</span>
              <strong style={{ color: "#ffffff", fontSize: isSmallScreen ? "0.7rem" : "0.8rem" }}>{selectedBay.nLoads}</strong>
            </div>
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline" }}>
              <span style={{ fontSize: isSmallScreen ? "0.4rem" : "0.55rem" }}>PRICE</span>
              <strong style={{ color: "#ed8200", fontSize: isSmallScreen ? "0.7rem" : "0.8rem" }}>{selectedBay.price}€</strong>
            </div>
          </div>
        </div>
      </div>

      <style>{`
        .bay-inspector-panel {
          position: absolute;
          bottom: ${offset}px;
          right: ${offset}px;
          top: auto;
          width: ${panelWidth}px;
          height: ${isSmallScreen ? panelHeight + 'px' : 'auto'};
          padding: 10px 12px;
          animation: panelSlideUp 0.15s ease-out;
          display: flex;
          flex-direction: column;
        }
        @keyframes panelSlideUp {
          from { opacity: 0; transform: scale(0.95); }
          to { opacity: 1; transform: scale(1); }
        }
      `}</style>
    </Html>
  );
}
