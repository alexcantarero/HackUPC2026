#pragma once

#include "types.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstddef>

// Forward declaration so CollisionChecker can accept SpatialGrid* before it is defined
class SpatialGrid;

// Oriented Bounding Box
struct OBB {
    Point2D corners[4]; // 4 corners of the rotated rectangle (BL, BR, TR, TL in local space)
    Point2D center;
};

/**
 * @brief Low-level collision primitives and high-level placement validation.
 *
 * Collision rules (from constitution):
 *   solid vs solid  = INVALID
 *   solid vs gap    = INVALID
 *   gap   vs solid  = INVALID
 *   gap   vs gap    = VALID  (bays may share aisle space)
 */
class CollisionChecker {
public:
    // --- OBB construction ---

    /** Creates the solid OBB for a bay. (x,y) is the pivot/bottom-left before rotation. */
    static OBB createSolidOBB(const Bay& bay, const StaticState* staticInfo);

    /**
     * @brief Creates the gap OBB for a bay.
     * The gap is the required aisle space on the depth+ face of the solid.
     * It spans the full width and extends `gap` units beyond the solid region.
     */
    static OBB createGapOBB(const Bay& bay, const StaticState* staticInfo);

    /** Creates an axis-aligned OBB for a rectangular obstacle. */
    static OBB createObstacleOBB(const Obstacle& obs);

    // --- High-level placement check ---

    /**
     * @brief Full validity check for placing a candidate bay.
     *
     * Verifies:
     *  1. Solid is fully inside the warehouse polygon
     *  2. Solid does not overlap any obstacle
     *  3. Gap does not overlap any obstacle
     *  4. Ceiling height satisfied across both solid and gap x-spans
     *  5. Solid does not overlap any placed bay's solid
     *  6. Solid does not overlap any placed bay's gap
     *  7. Gap does not overlap any placed bay's solid
     *  (Gap vs gap deliberately not checked — always valid)
     *
     * @param grid  Optional spatial grid for broad-phase narrowing. When provided,
     *              only nearby bays are SAT-checked instead of the full placed list.
     *              The grid must have been built with the solid OBBs of all placed bays.
     *              Pass nullptr to fall back to brute-force O(N) checking.
     */
    static bool isValidPlacement(
        const Bay& candidate,
        const std::vector<Bay>& placed,
        const StaticState* staticInfo,
        const SpatialGrid* grid = nullptr
    );

    // --- Low-level SAT primitives ---

    /** Returns true if two OBBs intersect (SAT). */
    static bool checkOBBvsOBB(const OBB& a, const OBB& b);

    /** Returns true if OBB is fully inside the warehouse polygon. */
    static bool isOBBInsidePolygon(const OBB& obb, const std::vector<Point2D>& polygon);

    /** Returns true if bay height fits under the ceiling across the solid's x-span. */
    static bool checkCeiling(const OBB& solidOBB, double bayHeight,
                              const std::vector<CeilingRegion>& ceilingRegions);

    /** Looks up a BayType by id. Returns nullptr if not found. */
    static const BayType* getBayType(int typeId, const StaticState* staticInfo);

private:
    static double dotProduct(const Point2D& a, const Point2D& b);
    static void   projectOBB(const OBB& obb, const Point2D& axis, double& min, double& max);
    static bool   overlap(double minA, double maxA, double minB, double maxB);
    static bool   isPointInsidePolygon(const Point2D& p, const std::vector<Point2D>& polygon);
    static bool   segmentsIntersect(const Point2D& p1, const Point2D& p2,
                                    const Point2D& p3, const Point2D& p4);

    /** Rotates 4 local corners around (pivotX, pivotY) and builds an OBB. */
    static OBB buildOBBFromLocalCorners(const Point2D local[4], double pivotX, double pivotY,
                                        double cosA, double sinA);
};

// --- SPATIAL HASH GRID ---

struct GridKey {
    int x, y;
    bool operator==(const GridKey& other) const {
        return x == other.x && y == other.y;
    }
};

struct GridKeyHash {
    std::size_t operator()(const GridKey& k) const {
        // Simple hash function for 2D grid coordinates
        return std::hash<int>()(k.x) ^ (std::hash<int>()(k.y) << 1);
    }
};

// Spatial Grid for O(1) amortized collision lookups
class SpatialGrid {
public:
    // cellSize should ideally be roughly the size of your largest bay
    SpatialGrid(double cellSize);
    
    // Insert a bay's OBB into the grid. 'bayIndex' is the index in the State::bays vector.
    void insertBay(int bayIndex, const OBB& obb);
    
    // Insert an obstacle into the grid. 'obsIndex' is the index in StaticState::obstacles.
    void insertObstacle(int obsIndex, const Obstacle& obs);
    
    // Returns indices of bays that MIGHT collide with the given OBB
    std::vector<int> getPotentialBayCollisions(const OBB& obb) const;
    void getPotentialBayCollisions(const OBB& obb, std::vector<int>& out_bays) const;
    
    // Returns indices of obstacles that MIGHT collide with the given OBB
    std::vector<int> getPotentialObstacleCollisions(const OBB& obb) const;

    // Clears all dynamic entities (bays). Obstacles can be kept if they are static.
    void clearBays();
    void clearAll();
    void removeBay(int bayIndex, const OBB &obb);


private:
    double cellSize;
    
    // Maps cell coordinates to list of bay/obstacle indices
    std::unordered_map<GridKey, std::vector<int>, GridKeyHash> gridBays;
    std::unordered_map<GridKey, std::vector<int>, GridKeyHash> gridObstacles;
    
    void getGridBounds(const OBB& obb, int& minX, int& maxX, int& minY, int& maxY) const;
};
