import * as THREE from "three";
import ProceduralShelf from "./ProceduralShelf";
import GapBox from "./GapBox";
import { WORLD_SCALE, FLOOR_Y } from "../../scene/buildSceneGeometry";
import type { WarehouseLayoutItem, BayType, WarehouseCenter } from "../../types/warehouse";

interface WarehouseProps {
  layoutData: WarehouseLayoutItem[];
  bayData: Record<number, BayType>;
  warehouseCenter: WarehouseCenter;
  showGaps: boolean;
  onSelectBay: (bayInfo: { id: number; nLoads: number; price: number; position: [number, number, number] } | null) => void;
}

export default function Warehouse({ layoutData, bayData, warehouseCenter, showGaps, onSelectBay }: WarehouseProps) {
  return (
    <group onPointerMissed={() => onSelectBay(null)}>
      {layoutData.map((item, index) => {
        const bay = bayData[item.id];
        if (!bay) return null;

        const boxWidth = bay.width * WORLD_SCALE;
        const boxHeight = bay.height * WORLD_SCALE;
        const boxDepth = bay.depth * WORLD_SCALE;
        const anchorX = item.x * WORLD_SCALE - warehouseCenter.centerX;
        const anchorZ = -(item.y * WORLD_SCALE - warehouseCenter.centerY);
        const rotationY = THREE.MathUtils.degToRad(item.rot);
        
        const hue = 40 + (item.id * 137.5) % 300;
        const color = `hsl(${hue}, 85%, 45%)`;

        // Calculate dead center for the arrow
        // anchorX, anchorZ is bottom-left (before rotation).
        // The center is width/2, height/2, -depth/2 (local).
        const center = new THREE.Vector3(boxWidth / 2, boxHeight / 2, -boxDepth / 2)
          .applyAxisAngle(new THREE.Vector3(0, 1, 0), rotationY)
          .add(new THREE.Vector3(anchorX, FLOOR_Y, anchorZ));

        return (
          <group
            key={`bay-${item.id}-${index}`}
            position={[anchorX, FLOOR_Y, anchorZ]}
            rotation={[0, rotationY, 0]}
            onPointerOver={(e) => {
              e.stopPropagation();
              onSelectBay({
                id: item.id,
                nLoads: bay.nLoads,
                price: bay.price,
                position: [center.x, center.y, center.z]
              });
            }}
            onPointerOut={() => {
              onSelectBay(null);
            }}
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
    </group>
  );
}
