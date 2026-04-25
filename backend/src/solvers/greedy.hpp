#pragma once
#include "algorithm.hpp"
#include "../core/score.hpp"
#include <array>
#include <set>

/**
 * @brief Constructive BLF (Bottom-Left Fill) greedy solver.
 *
 * Maintains a sorted candidate set of insertion points derived from
 * previously placed bay corners. At each point, tries every bay type
 * (largest first) across 13 rotation angles (0-180 degrees, step 15).
 *
 * fillPass() is public so SimulatedAnnealing can call it directly as
 * a warm-start without going through run().
 */
class GreedySolver : public Algorithm {
public:
    GreedySolver(const StaticState& info, uint64_t seed);

    void        run(std::atomic<bool>& stop_flag) override;
    std::string name() const override { return "greedy"; }

    /**
     * @brief Run one BLF fill pass into best_.bays.
     * Resets the layout first (best_.bays cleared, grid cleared).
     * Computes and stores best_.score after placement.
     */
    void fillPass();

private:
    double           wh_area_;
    std::vector<int> sorted_type_ids_; ///< indices into info_.bayTypes, desc by area
    std::array<double, 13> rotations_; ///< 0, 15, 30, ..., 180 degrees

    struct PointCmp {
        bool operator()(const Point2D& a, const Point2D& b) const {
            if (a.y != b.y) return a.y < b.y;
            return a.x < b.x;
        }
    };
    using CandidateSet = std::set<Point2D, PointCmp>;
};
