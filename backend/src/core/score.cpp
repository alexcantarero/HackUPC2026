#include "score.hpp"
#include <cmath>

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

double computeScore(const std::vector<Bay>& bays,
                    const StaticState& info,
                    double wh_area) {
    if (bays.empty() || wh_area <= 0.0) return std::numeric_limits<double>::max();

    double sum_ratio = 0.0;
    double sum_area  = 0.0;
    
    for (const auto& bay : bays) {
        for (const auto& bt : info.bayTypes) {
            if (bt.id == bay.typeId) {
                sum_ratio += (bt.price / bt.nLoads); // Summing the ratios
                sum_area  += (bt.width * bt.depth);
                break;
            }
        }
    }
    
    // As sum_area approaches wh_area, exponent approaches 1.0
    double exponent = 2.0 - (sum_area / wh_area); 
    
    return std::pow(sum_ratio, exponent);
}