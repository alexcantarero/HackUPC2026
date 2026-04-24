#pragma once

#include "State.hpp"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstddef>

// Oriented Bounding Box
struct OBB {
    Point2D corners[4]; // 4 corners of the rotated rectangle
    Point2D center;     // Center of the rectangle
};

class CollisionChecker {
public:
    // Creates an OBB from a Bay and StaticState.
    // Handles (x,y) as the bottom-left corner before rotation.
    // Rotation is applied around this bottom-left corner.
    static OBB createOBB(const Bay& bay, const StaticState* staticInfo);

    // Creates an OBB for an Obstacle (Axis-Aligned, rotation = 0)
    static OBB createOBB(const Obstacle& obs);

    // Separating Axis Theorem to check intersection between two OBBs
    static bool checkOBBvsOBB(const OBB& a, const OBB& b);

    // Checks if an OBB intersects with an Axis Aligned Bounding Box (like an Obstacle)
    static bool checkOBBvsAABB(const OBB& obb, const Obstacle& obs);

    // Checks if the OBB is fully inside the warehouse polygon
    static bool isOBBInsidePolygon(const OBB& obb, const std::vector<Point2D>& polygon);

    // Checks if the Bay satisfies the ceiling constraints
    static bool checkCeiling(const OBB& obb, double bayHeight, const std::vector<CeilingRegion>& ceilingRegions);

    // Utility to get a BayType by ID
    static const BayType* getBayType(int typeId, const StaticState* staticInfo);

private:
    static double dotProduct(const Point2D& a, const Point2D& b);
    static void projectOBB(const OBB& obb, const Point2D& axis, double& min, double& max);
    static bool overlap(double minA, double maxA, double minB, double maxB);
    static bool isPointInsidePolygon(const Point2D& p, const std::vector<Point2D>& polygon);
    static bool segmentsIntersect(const Point2D& p1, const Point2D& p2, const Point2D& p3, const Point2D& p4);
};

// --- SPATIAL HASH GRID OPTIMIZATION ---

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
    // Uses the largestBaySize from StaticState for the cell size
    SpatialGrid(const StaticState* staticInfo);
    
    // Insert a bay's OBB into the grid. 'bayIndex' is the index in the State::bays vector.
    void insertBay(int bayIndex, const OBB& obb);
    
    // Insert an obstacle into the grid. 'obsIndex' is the index in StaticState::obstacles.
    void insertObstacle(int obsIndex, const Obstacle& obs);
    
    // Returns indices of bays that MIGHT collide with the given OBB
    std::vector<int> getPotentialBayCollisions(const OBB& obb) const;
    
    // Returns indices of obstacles that MIGHT collide with the given OBB
    std::vector<int> getPotentialObstacleCollisions(const OBB& obb) const;

    // Clears all dynamic entities (bays). Obstacles can be kept if they are static.
    void clearBays();
    void clearAll();

private:
    double cellSize;
    
    // Maps cell coordinates to list of bay/obstacle indices
    std::unordered_map<GridKey, std::vector<int>, GridKeyHash> gridBays;
    std::unordered_map<GridKey, std::vector<int>, GridKeyHash> gridObstacles;
    
    void getGridBounds(const OBB& obb, int& minX, int& maxX, int& minY, int& maxY) const;
};
