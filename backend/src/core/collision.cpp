#include "collision.hpp"
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

OBB CollisionChecker::buildOBBFromLocalCorners(const Point2D local[4], double pivotX, double pivotY,
                                                double cosA, double sinA) {
    OBB obb;
    double cx = 0.0, cy = 0.0;
    for (int i = 0; i < 4; ++i) {
        obb.corners[i].x = pivotX + (local[i].x * cosA - local[i].y * sinA);
        obb.corners[i].y = pivotY + (local[i].x * sinA + local[i].y * cosA);
        cx += obb.corners[i].x;
        cy += obb.corners[i].y;
    }
    obb.center = {cx / 4.0, cy / 4.0};
    return obb;
}

OBB CollisionChecker::createSolidOBB(const Bay& bay, const StaticState* staticInfo) {
    const BayType* type = getBayType(bay.typeId, staticInfo);
    if (!type) {
        OBB obb;
        for (int i = 0; i < 4; ++i) obb.corners[i] = {bay.x, bay.y};
        obb.center = {bay.x, bay.y};
        return obb;
    }

    const double w    = type->width;
    const double d    = type->depth;
    const double cosA = std::cos(degToRad(bay.rotation));
    const double sinA = std::sin(degToRad(bay.rotation));

    // Solid occupies local y = [0, depth]
    Point2D local[4] = {{0, 0}, {w, 0}, {w, d}, {0, d}};
    return buildOBBFromLocalCorners(local, bay.x, bay.y, cosA, sinA);
}

OBB CollisionChecker::createGapOBB(const Bay& bay, const StaticState* staticInfo) {
    const BayType* type = getBayType(bay.typeId, staticInfo);
    if (!type) {
        OBB obb;
        for (int i = 0; i < 4; ++i) obb.corners[i] = {bay.x, bay.y};
        obb.center = {bay.x, bay.y};
        return obb;
    }

    const double w    = type->width;
    const double d    = type->depth;
    const double g    = type->gap;
    const double cosA = std::cos(degToRad(bay.rotation));
    const double sinA = std::sin(degToRad(bay.rotation));

    // Gap occupies local y = [depth, depth + gap] — the aisle in front of the solid
    Point2D local[4] = {{0, d}, {w, d}, {w, d + g}, {0, d + g}};
    return buildOBBFromLocalCorners(local, bay.x, bay.y, cosA, sinA);
}

OBB CollisionChecker::createObstacleOBB(const Obstacle& obs) {
    OBB obb;
    obb.corners[0] = {obs.x,             obs.y};
    obb.corners[1] = {obs.x + obs.width, obs.y};
    // Top-Right
    obb.corners[2] = {obs.x + obs.width, obs.y + obs.depth};
    // Top-Left
    obb.corners[3] = {obs.x, obs.y + obs.depth};

    obb.center.x = obs.x + obs.width / 2.0;
    obb.center.y = obs.y + obs.depth / 2.0;

    return obb;
}

bool CollisionChecker::isValidPlacement(
    const Bay& candidate,
    const std::vector<Bay>& placed,
    const StaticState* staticInfo,
    const SpatialGrid* grid)
{
    const BayType* type = getBayType(candidate.typeId, staticInfo);
    if (!type) return false;

    const OBB solidOBB = createSolidOBB(candidate, staticInfo);
    const OBB gapOBB   = createGapOBB(candidate, staticInfo);

    // 1. Solid and gap must be fully inside the warehouse polygon
    if (!isOBBInsidePolygon(solidOBB, staticInfo->warehousePolygon)) return false;
    if (!isOBBInsidePolygon(gapOBB, staticInfo->warehousePolygon)) return false;

    // 2. Ceiling constraint on solid and gap.
    // The gap is the forklift aisle — if the ceiling is too low there, the bay
    // is inaccessible even if the solid itself fits.
    if (!checkCeiling(solidOBB, type->height, staticInfo->ceilingRegions)) return false;
    if (!checkCeiling(gapOBB,   type->height, staticInfo->ceilingRegions)) return false;

    // 3 & 4. Solid and gap must not overlap any obstacle
    for (const auto& obs : staticInfo->obstacles) {
        OBB obsOBB = createObstacleOBB(obs);
        if (checkOBBvsOBB(solidOBB, obsOBB)) return false;
        if (checkOBBvsOBB(gapOBB,   obsOBB)) return false;
    }

    // 5, 6, 7. Inter-bay collision checks (gap vs gap is intentionally skipped).
    // Broad-phase: query grid with both solid and gap OBBs to get nearby candidates,
    // then narrow-phase SAT only against those. Falls back to O(N) without a grid.
    if (grid) {
        auto solidCands = grid->getPotentialBayCollisions(solidOBB);
        auto gapCands   = grid->getPotentialBayCollisions(gapOBB);

        // Union both candidate sets (grid is keyed on solid OBBs, querying with both
        // OBBs ensures we catch: other-solid-near-our-solid and other-solid-near-our-gap)
        std::unordered_set<int> candidates(solidCands.begin(), solidCands.end());
        candidates.insert(gapCands.begin(), gapCands.end());

        for (int idx : candidates) {
            const OBB otherSolid = createSolidOBB(placed[idx], staticInfo);
            const OBB otherGap   = createGapOBB(placed[idx], staticInfo);

            if (checkOBBvsOBB(solidOBB, otherSolid)) return false; // solid vs solid
            if (checkOBBvsOBB(solidOBB, otherGap))   return false; // solid vs gap
            if (checkOBBvsOBB(gapOBB,   otherSolid)) return false; // gap vs solid
        }
    } else {
        for (const auto& other : placed) {
            const OBB otherSolid = createSolidOBB(other, staticInfo);
            const OBB otherGap   = createGapOBB(other, staticInfo);

            if (checkOBBvsOBB(solidOBB, otherSolid)) return false; // solid vs solid
            if (checkOBBvsOBB(solidOBB, otherGap))   return false; // solid vs gap
            if (checkOBBvsOBB(gapOBB,   otherSolid)) return false; // gap vs solid
        }
    }

    return true;
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
    // Strict: touching edges (minA == maxB) are NOT considered overlapping.
    // The problem spec allows bays to share boundaries.
    // We use a small epsilon to account for floating-point inaccuracies.
    const double EPSILON = 1e-5;
    return !(minA >= maxB - EPSILON || maxA <= minB + EPSILON);
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

SpatialGrid::SpatialGrid(double cellSize) : cellSize(cellSize) {}

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
    OBB obb = CollisionChecker::createObstacleOBB(obs);
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
