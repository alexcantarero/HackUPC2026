interface ProceduralShelfProps {
  width: number;
  height: number;
  depth: number;
  color?: string;
}

export default function ProceduralShelf({
  width,
  height,
  depth,
  color = "#3a82f7",
}: ProceduralShelfProps) {
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
