#pragma once
#include "types.hpp"
#include <vector>

// Polygon area via the shoelace formula. Pass the warehousePolygon.
double warehouseArea(const std::vector<Point2D>& polygon);

double computeTrainingScore(const std::vector<Bay>& bays, const StaticState& info, double wh_area);
double computeOfficialScore(const std::vector<Bay>& bays, const StaticState& info, double wh_area);
