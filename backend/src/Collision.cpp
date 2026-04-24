#include "Collision.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper to convert degrees to radians
static double degToRad(double degrees) {
    return degrees * M_PI / 180.0;
}

const BayType* CollisionChecker::getBayType(int typeId, const StaticState* staticInfo) {
    for (const auto& bt : staticInfo->bayTypes) {
        if (bt.id == typeId) return &bt;
    }
    return nullptr;
}

OBB CollisionChecker::createOBB(const Bay& bay, const StaticState* staticInfo) {
    OBB obb;
    const BayType* type = getBayType(bay.typeId, staticInfo);
    if (!type) {
        // Fallback if ID is not found
        for (int i = 0; i < 4; ++i) obb.corners[i] = {bay.x, bay.y};
        obb.center = {bay.x, bay.y};
        return obb;
    }

    double rad = degToRad(bay.rotation);
    double cosA = std::cos(rad);
    double sinA = std::sin(rad);

    // Dimensions
    double w = type->width;
    double d = type->depth;

    // Local corners before rotation, assuming (0,0) is the bottom-left corner
    Point2D local[4] = {
        {0, 0},     // Bottom Left (Pivot)
        {w, 0},     // Bottom Right
        {w, d},     // Top Right
        {0, d}      // Top Left
    };

    double cx = 0.0, cy = 0.0;

    // Rotate around bottom-left (0,0) and translate by (bay.x, bay.y)
    for (int i = 0; i < 4; ++i) {
        obb.corners[i].x = bay.x + (local[i].x * cosA - local[i].y * sinA);
        obb.corners[i].y = bay.y + (local[i].x * sinA + local[i].y * cosA);
        cx += obb.corners[i].x;
        cy += obb.corners[i].y;
    }

    // Calculate and store the center
    obb.center.x = cx / 4.0;
    obb.center.y = cy / 4.0;

    return obb;
}

OBB CollisionChecker::createOBB(const Obstacle& obs) {
    OBB obb;
    // Bottom-Left
    obb.corners[0] = {obs.x, obs.y};
    // Bottom-Right
    obb.corners[1] = {obs.x + obs.width, obs.y};
    // Top-Right
    obb.corners[2] = {obs.x + obs.width, obs.y + obs.height};
    // Top-Left
    obb.corners[3] = {obs.x, obs.y + obs.height};

    obb.center.x = obs.x + obs.width / 2.0;
    obb.center.y = obs.y + obs.height / 2.0;

    return obb;
}

double CollisionChecker::dotProduct(const Point2D& a, const Point2D& b) {
    return a.x * b.x + a.y * b.y;
}

void CollisionChecker::projectOBB(const OBB& obb, const Point2D& axis, double& min, double& max) {
    min = dotProduct(obb.corners[0], axis);
    max = min;
    for (int i = 1; i < 4; ++i) {
        double p = dotProduct(obb.corners[i], axis);
        if (p < min) min = p;
        if (p > max) max = p;
    }
}

bool CollisionChecker::overlap(double minA, double maxA, double minB, double maxB) {
    return !(minA > maxB || maxA < minB);
}

bool CollisionChecker::checkOBBvsOBB(const OBB& a, const OBB& b) {
    const OBB* boxes[2] = { &a, &b };
    
    for (int i = 0; i < 2; ++i) {
        const OBB* box = boxes[i];
        for (int j = 0; j < 2; ++j) {
            int corner1 = j;
            int corner2 = (j + 1) % 4;

            Point2D edge = { box->corners[corner2].x - box->corners[corner1].x,
                             box->corners[corner2].y - box->corners[corner1].y };

            Point2D axis = { -edge.y, edge.x };

            double minA, maxA, minB, maxB;
            projectOBB(a, axis, minA, maxA);
            projectOBB(b, axis, minB, maxB);

            if (!overlap(minA, maxA, minB, maxB)) {
                return false;
            }
        }
    }
    return true;
}

bool CollisionChecker::checkOBBvsAABB(const OBB& obb, const Obstacle& obs) {
    OBB obsObb = createOBB(obs);
    return checkOBBvsOBB(obb, obsObb);
}

bool CollisionChecker::isPointInsidePolygon(const Point2D& p, const std::vector<Point2D>& polygon) {
    bool inside = false;
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        if (((polygon[i].y > p.y) != (polygon[j].y > p.y)) &&
            (p.x < (polygon[j].x - polygon[i].x) * (p.y - polygon[i].y) / (polygon[j].y - polygon[i].y) + polygon[i].x)) {
            inside = !inside;
        }
    }
    return inside;
}

static bool ccw(const Point2D& A, const Point2D& B, const Point2D& C) {
    return (C.y - A.y) * (B.x - A.x) > (B.y - A.y) * (C.x - A.x);
}

bool CollisionChecker::segmentsIntersect(const Point2D& p1, const Point2D& p2, const Point2D& p3, const Point2D& p4) {
    return ccw(p1, p3, p4) != ccw(p2, p3, p4) && ccw(p1, p2, p3) != ccw(p1, p2, p4);
}

bool CollisionChecker::isOBBInsidePolygon(const OBB& obb, const std::vector<Point2D>& polygon) {
    if (polygon.size() < 3) return false;

    for (int i = 0; i < 4; ++i) {
        if (!isPointInsidePolygon(obb.corners[i], polygon)) {
            return false;
        }
    }

    for (int i = 0; i < 4; ++i) {
        Point2D obbP1 = obb.corners[i];
        Point2D obbP2 = obb.corners[(i + 1) % 4];

        for (size_t j = 0; j < polygon.size(); ++j) {
            Point2D polyP1 = polygon[j];
            Point2D polyP2 = polygon[(j + 1) % polygon.size()];

            if (segmentsIntersect(obbP1, obbP2, polyP1, polyP2)) {
                return false;
            }
        }
    }
    return true;
}

bool CollisionChecker::checkCeiling(const OBB& obb, double bayHeight, const std::vector<CeilingRegion>& ceilingRegions) {
    if (ceilingRegions.empty()) return true;

    double minX = obb.corners[0].x;
    double maxX = obb.corners[0].x;
    for (int i = 1; i < 4; ++i) {
        if (obb.corners[i].x < minX) minX = obb.corners[i].x;
        if (obb.corners[i].x > maxX) maxX = obb.corners[i].x;
    }

    for (size_t i = 0; i < ceilingRegions.size(); ++i) {
        double regionStartX = ceilingRegions[i].x;
        double regionEndX = (i + 1 < ceilingRegions.size()) ? ceilingRegions[i+1].x : std::numeric_limits<double>::max();

        if (maxX > regionStartX && minX < regionEndX) {
            if (bayHeight > ceilingRegions[i].height) {
                return false;
            }
        }
    }
    return true;
}

// --- SPATIAL HASH GRID IMPLEMENTATION ---

SpatialGrid::SpatialGrid(const StaticState* staticInfo) {
    // Determine cell size based on the largest bay dimension. Default to 1.0 if not set.
    cellSize = (staticInfo && staticInfo->largestBaySize > 0.0) ? staticInfo->largestBaySize : 1.0;
}

void SpatialGrid::getGridBounds(const OBB& obb, int& minX, int& maxX, int& minY, int& maxY) const {
    double min_x = obb.corners[0].x, max_x = obb.corners[0].x;
    double min_y = obb.corners[0].y, max_y = obb.corners[0].y;
    
    for (int i = 1; i < 4; ++i) {
        if (obb.corners[i].x < min_x) min_x = obb.corners[i].x;
        if (obb.corners[i].x > max_x) max_x = obb.corners[i].x;
        if (obb.corners[i].y < min_y) min_y = obb.corners[i].y;
        if (obb.corners[i].y > max_y) max_y = obb.corners[i].y;
    }

    minX = static_cast<int>(std::floor(min_x / cellSize));
    maxX = static_cast<int>(std::floor(max_x / cellSize));
    minY = static_cast<int>(std::floor(min_y / cellSize));
    maxY = static_cast<int>(std::floor(max_y / cellSize));
}

void SpatialGrid::insertBay(int bayIndex, const OBB& obb) {
    int minX, maxX, minY, maxY;
    getGridBounds(obb, minX, maxX, minY, maxY);

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            gridBays[{x, y}].push_back(bayIndex);
        }
    }
}

void SpatialGrid::insertObstacle(int obsIndex, const Obstacle& obs) {
    OBB obb = CollisionChecker::createOBB(obs);
    int minX, maxX, minY, maxY;
    getGridBounds(obb, minX, maxX, minY, maxY);

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            gridObstacles[{x, y}].push_back(obsIndex);
        }
    }
}

std::vector<int> SpatialGrid::getPotentialBayCollisions(const OBB& obb) const {
    int minX, maxX, minY, maxY;
    getGridBounds(obb, minX, maxX, minY, maxY);

    std::unordered_set<int> uniqueBays;
    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            GridKey key = {x, y};
            auto it = gridBays.find(key);
            if (it != gridBays.end()) {
                for (int bayIndex : it->second) {
                    uniqueBays.insert(bayIndex);
                }
            }
        }
    }

    return std::vector<int>(uniqueBays.begin(), uniqueBays.end());
}

std::vector<int> SpatialGrid::getPotentialObstacleCollisions(const OBB& obb) const {
    int minX, maxX, minY, maxY;
    getGridBounds(obb, minX, maxX, minY, maxY);

    std::unordered_set<int> uniqueObs;
    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            GridKey key = {x, y};
            auto it = gridObstacles.find(key);
            if (it != gridObstacles.end()) {
                for (int obsIndex : it->second) {
                    uniqueObs.insert(obsIndex);
                }
            }
        }
    }

    return std::vector<int>(uniqueObs.begin(), uniqueObs.end());
}

void SpatialGrid::clearBays() {
    gridBays.clear();
}

void SpatialGrid::clearAll() {
    gridBays.clear();
    gridObstacles.clear();
}
