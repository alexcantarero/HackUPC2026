#include "score.hpp"
#include "collision.hpp"
#include <cmath>
#include <limits>

double warehouseArea(const std::vector<Point2D>& polygon) {
    double area = 0.0;
    const int n = static_cast<int>(polygon.size());
    for (int i = 0; i < n; ++i) {
        const Point2D& a = polygon[i];
        const Point2D& b = polygon[(i + 1) % n];
        area += a.x * b.y - b.x * a.y;
    }
    return std::fabs(area) / 2.0;
}

// The Monotonic Objective (Q_new) to smoothly guide the algorithms
double computeTrainingScore(const std::vector<Bay>& bays, const StaticState& info, double wh_area) {
    if (bays.empty() || wh_area <= 0.0) return std::numeric_limits<double>::max();
    double sum_ratio = 0.0, sum_area = 0.0;
    for (const auto& bay : bays) {
        const BayType* bt = CollisionChecker::getBayType(bay.typeId, &info);
        if (bt) {
            sum_ratio += (bt->price / bt->nLoads);
            sum_area  += (bt->width * bt->depth);
        }
    }
    if (sum_area <= 0.0) return std::numeric_limits<double>::max();
    double area_fraction = wh_area / sum_area;
    return sum_ratio * (area_fraction * area_fraction);
}

// The Judge's Exact Formula (Q_old)
double computeOfficialScore(const std::vector<Bay>& bays, const StaticState& info, double wh_area) {
    if (bays.empty() || wh_area <= 0.0) return std::numeric_limits<double>::max();
    double sum_ratio = 0.0, sum_area = 0.0;
    for (const auto& bay : bays) {
        const BayType* bt = CollisionChecker::getBayType(bay.typeId, &info);
        if (bt) {
            sum_ratio += (bt->price / bt->nLoads);
            sum_area  += (bt->width * bt->depth);
        }
    }
    if (sum_ratio <= 0.0) return std::numeric_limits<double>::max();
    
    double exponent = 2.0 - (sum_area / wh_area);
    if (exponent < 1.0) exponent = 1.0; // Safety cap
    return std::pow(sum_ratio, exponent);
}