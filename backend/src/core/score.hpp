#pragma once
#include "types.hpp"
#include <vector>

// Polygon area via the shoelace formula. Pass the warehousePolygon.
double warehouseArea(const std::vector<Point2D>& polygon);

// Q = (Σ_bay price/nLoads)^2 − (Σ_bay width*depth / wh_area).
// Lower is better. Pass the result of warehouseArea() as wh_area.
double computeScore(const std::vector<Bay>& bays,
                    const StaticState& info,
                    double wh_area);
