#include "greedy.hpp"
#include "../core/collision.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
void expandBounds(const OBB& obb, double& min_x, double& max_x, double& min_y, double& max_y) {
    for (int i = 0; i < 4; ++i) {
        min_x = std::min(min_x, obb.corners[i].x); max_x = std::max(max_x, obb.corners[i].x);
        min_y = std::min(min_y, obb.corners[i].y); max_y = std::max(max_y, obb.corners[i].y);
    }
}
}

GreedySolver::GreedySolver(const StaticState& info, uint64_t seed)
    : Algorithm(info, seed)
{
    for (int i = 0; i < static_cast<int>(info_.bayTypes.size()); ++i)
        sorted_type_ids_.push_back(i);
        
    std::sort(sorted_type_ids_.begin(), sorted_type_ids_.end(),
              [&](int a, int b) {
                  const auto& ta = info_.bayTypes[a];
                  const auto& tb = info_.bayTypes[b];
                  
                  // CRITICAL FIX: Sort by Efficiency (Area / (Price / Loads))
                  // This forces Greedy to pack highly profitable bays first!
                  double eff_a = (ta.width * ta.depth) / (ta.price / ta.nLoads);
                  double eff_b = (tb.width * tb.depth) / (tb.price / tb.nLoads);
                  return eff_a > eff_b;
              });

    for (int i = 0; i < 13; ++i)
        rotations_[i] = static_cast<double>(i) * 15.0;
}

void GreedySolver::run(std::atomic<bool>& stop_flag) {
    (void)stop_flag; 
    fillPass();
}

void GreedySolver::fillPass() {
    resetLayout(); 
    if (info_.warehousePolygon.empty() || info_.bayTypes.empty()) return;

    Solution best_prefix = best_;
    double best_prefix_score = evaluateTraining(best_.bays);

    std::vector<Point2D> anchors = generateSafeAnchors();
    SpatialGrid decode_grid(largestBayDim(info_));
    constexpr int kMaxAnchors = 500;

    bool placed_any_in_pass = true;
    
    while (placed_any_in_pass) {
        placed_any_in_pass = false;

        std::sort(anchors.begin(), anchors.end(),[](const Point2D& lhs, const Point2D& rhs) {
            auto quantize =[](double v) { return static_cast<long long>(std::round(v * 100.0)); };
            long long q_ly = quantize(lhs.y);
            long long q_ry = quantize(rhs.y);
            if (q_ly != q_ry) return q_ly < q_ry;
            return quantize(lhs.x) < quantize(rhs.x);
        });

        for (int type_idx : sorted_type_ids_) {
            const BayType& bt = info_.bayTypes[type_idx];
            bool bay_placed = false;
            Bay best_candidate{bt.id, 0.0, 0.0, 0.0};

            for (const auto& anchor : anchors) {
                for (double angle : {0.0, 90.0, 180.0, 270.0}) {
                    Bay candidate{bt.id, anchor.x, anchor.y, angle};
                    if (CollisionChecker::isValidPlacement(candidate, best_.bays, &info_, &decode_grid)) {
                        best_candidate = candidate;
                        bay_placed = true;
                        break;
                    }
                }
                if (bay_placed) break; 
            }

            if (bay_placed) {
                best_.bays.push_back(best_candidate);
                OBB bound = CollisionChecker::createBoundingOBB(best_candidate, &info_);
                decode_grid.insertBay(static_cast<int>(best_.bays.size() - 1), bound);

                double cur_score = evaluateTraining(best_.bays);
                if (cur_score < best_prefix_score) {
                    best_prefix_score = cur_score;
                    best_prefix = best_;
                }

                addBayAnchors(best_candidate, anchors);

                if (anchors.size() > static_cast<size_t>(kMaxAnchors)) {
                    anchors.erase(anchors.begin(), anchors.begin() + (anchors.size() - kMaxAnchors));
                }

                placed_any_in_pass = true;
                break; 
            }
        }
    }

    best_ = best_prefix;
    calculateMetrics(best_);
    best_.producedBy = name();
    updateBest(best_);
}