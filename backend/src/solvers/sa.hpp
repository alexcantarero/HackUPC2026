// #pragma once
// #include "algorithm.hpp"
// #include "../core/score.hpp"
// #include "greedy.hpp"
// #include <array>
// #include <vector>

// /**
//  * @brief Simulated Annealing solver.
//  *
//  * Phase 1: runs GreedySolver::fillPass() as a warm start.
//  * Phase 2: iterates Metropolis moves (relocate / replace-type 50/50)
//  *          with exponential cooling T *= 0.9995 until stop_flag fires.
//  *
//  * best_ (Algorithm base member) is updated every time an accepted move
//  * improves the score, so it is always safe to read after the thread joins.
//  *
//  * Incremental sums (sum_ratio_, sum_area_) let each move recompute Q in O(1).
//  */
// class SimulatedAnnealing : public Algorithm {
// public:
//     SimulatedAnnealing(const StaticState& info, uint64_t seed);

//     void        run(std::atomic<bool>& stop_flag) override;
//     std::string name() const override { return "sa"; }

// private:
//     // ── Working-layout state (separate from best_) ───────────────────────
//     double              wh_area_;
//     double              sum_ratio_;      ///< Σ price/nLoads for current layout
//     double              sum_area_;       ///< Σ width*depth  for current layout
//     std::vector<Bay>    current_bays_;
//     SpatialGrid         current_grid_;   ///< broad-phase grid for current_bays_
//     std::vector<Point2D> event_points_;  ///< solid OBB corners of current bays

//     // ── Helpers ──────────────────────────────────────────────────────────
//     double currentScore() const;
//     void   initFromBays(const std::vector<Bay>& bays);
//     void   rebuildEventPoints();
//     double calibrateT0();

//     // Move primitives — return true if move was accepted
//     bool moveRelocate(double T);
//     bool moveReplaceType(double T);

//     // Incremental helpers
//     double bayRatio(int typeId) const;
//     double bayArea (int typeId) const;

//     // Removes current_bays_[idx] and rebuilds current_grid_ to match.
//     // The bay is returned; current_bays_ has N-1 elements afterwards.
//     Bay removeBayAt(int idx);

//     // Appends a bay to current_bays_ and inserts it into current_grid_.
//     void appendBay(const Bay& bay);

//     static double largestBayDim(const StaticState& info);
// };