#include "score.hpp"
#include "collision.hpp"
#include <cmath>
#include <limits>
#include <iostream>

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
    double score = sum_ratio * (area_fraction * area_fraction);
    
    // Protect against NaN crashes in std::sort
    if (std::isnan(score)) {
        std::cout << "Warning: NaN score encountered. sum_ratio=" << sum_ratio << ", sum_area=" << sum_area << ", wh_area=" << wh_area << "\n"; 
        return std::numeric_limits<double>::max();
    }
    return score;
}

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
    if (exponent < 1.0) exponent = 1.0; 
    
    double score = std::pow(sum_ratio, exponent);
    if (std::isnan(score)) return std::numeric_limits<double>::max();
    return score;
}