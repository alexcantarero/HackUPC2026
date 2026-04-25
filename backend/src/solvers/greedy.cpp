#include "greedy.hpp"
#include "../core/collision.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

GreedySolver::GreedySolver(const StaticState& info, uint64_t seed)
    : Algorithm(info, seed)
    , wh_area_(warehouseArea(info.warehousePolygon))
{
    // Sort bay type indices by footprint area descending (largest first)
    for (int i = 0; i < static_cast<int>(info_.bayTypes.size()); ++i)
        sorted_type_ids_.push_back(i);
    std::sort(sorted_type_ids_.begin(), sorted_type_ids_.end(),
              [&](int a, int b) {
                  const auto& ta = info_.bayTypes[a];
                  const auto& tb = info_.bayTypes[b];
                  return (ta.width * ta.depth) > (tb.width * tb.depth);
              });

    // Precompute 13 rotation angles: 0°, 15°, ..., 180°
    for (int i = 0; i < 13; ++i)
        rotations_[i] = static_cast<double>(i) * 15.0;
}

void GreedySolver::run(std::atomic<bool>& stop_flag) {
    (void)stop_flag;
    fillPass();
    updateBest(best_);
}

void GreedySolver::fillPass() {
    resetLayout(); // clear best_.bays and grid_

    if (info_.warehousePolygon.empty() || info_.bayTypes.empty()) return;

    // Seed candidate set with warehouse AABB bottom-left corner
    double min_x = info_.warehousePolygon[0].x;
    double min_y = info_.warehousePolygon[0].y;
    for (const auto& p : info_.warehousePolygon) {
        if (p.x < min_x) min_x = p.x;
        if (p.y < min_y) min_y = p.y;
    }

    CandidateSet candidates;
    candidates.insert({min_x, min_y});

    // Also seed with obstacle corners (potential tight-packing spots)
    for (const auto& obs : info_.obstacles) {
        candidates.insert({obs.x,             obs.y});
        candidates.insert({obs.x + obs.width, obs.y});
        candidates.insert({obs.x,             obs.y + obs.depth});
        candidates.insert({obs.x + obs.width, obs.y + obs.depth});
    }

    std::array<double, 13> rots = rotations_;

    while (!candidates.empty()) {
        // Pop the bottom-left candidate (smallest y, then smallest x)
        Point2D pos = *candidates.begin();
        candidates.erase(candidates.begin());

        bool placed = false;
        for (int type_idx : sorted_type_ids_) {
            if (placed) break;
            const BayType& bt = info_.bayTypes[type_idx];

            // Shuffle rotations per position for inter-thread diversity
            std::shuffle(rots.begin(), rots.end(), rng_);

            for (double rot : rots) {
                Bay candidate{bt.id, pos.x, pos.y, rot};
                if (tryPlace(candidate)) {
                    // Add the new bay's solid corners as future candidates
                    OBB solid = CollisionChecker::createSolidOBB(candidate, &info_);
                    for (int i = 0; i < 4; ++i)
                        candidates.insert(solid.corners[i]);
                    placed = true;
                    break;
                }
            }
        }
    }

    calculateMetrics(best_);
    best_.producedBy = name();
}