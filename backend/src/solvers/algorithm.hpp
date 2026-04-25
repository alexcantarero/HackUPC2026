#pragma once

#include "../core/types.hpp"
#include "../core/collision.hpp"
#include <atomic>
#include <random>
#include <string>
#include <algorithm>
#include <chrono>

/**
 * @brief Abstract base class for all placement algorithms.
 *
 * Each algorithm runs on its own thread, loops until stop_flag is set,
 * and keeps its personal best Solution in best_. The main thread reads
 * best() only after joining — no locking required.
 *
 * Subclasses must implement:
 *   - run(stop_flag)   — the optimization loop
 *   - name()           — human-readable identifier
 *
 * Shared utilities provided by the base:
 *   - tryPlace(bay)    — validate + insert into best_.bays and grid_
 *   - resetLayout(bays)— rebuild grid from an arbitrary bay list
 */
class Algorithm {
public:
    /**
     * @param info  Problem constants (polygon, obstacles, ceiling, bay types).
     *              Must outlive this object — stored by reference.
     * @param seed  RNG seed. Use different seeds per thread for parallel mode.
     */
    Algorithm(const StaticState& info, uint64_t seed)
        : info_(info)
        , rng_(seed)
        , grid_(largestBayDim(info))
        , startTime_(std::chrono::steady_clock::now())
    {}

    virtual ~Algorithm() = default;

    // Non-copyable — each thread owns its own instance
    Algorithm(const Algorithm&)            = delete;
    Algorithm& operator=(const Algorithm&) = delete;

    /** Runs the optimization loop. Blocks until stop_flag is true. */
    virtual void run(std::atomic<bool>& stop_flag) = 0;

    /** Human-readable algorithm name. */
    virtual std::string name() const = 0;

    /**
     * @brief Best valid solution found so far.
     * Safe to read only after run() has returned (i.e. after thread join).
     */
    const Solution& best() const { return best_; }

protected:
    // -------------------------------------------------------------------------
    // Utility: placement helpers used by most algorithms
    // -------------------------------------------------------------------------

    /**
     * @brief Validate and place a bay into the current layout.
     * Updates best_.bays and grid_ atomically (within this thread).
     * @return true if the bay was placed successfully.
     */
    bool tryPlace(const Bay& bay) {
        if (!CollisionChecker::isValidPlacement(bay, best_.bays, &info_, &grid_))
            return false;
        grid_.insertBay(static_cast<int>(best_.bays.size()),
                        CollisionChecker::createSolidOBB(bay, &info_));
        best_.bays.push_back(bay);
        return true;
    }

    /**
     * @brief Replace the current layout with a new bay list and rebuild the grid.
     * Used by algorithms that mutate whole layouts (SA, Jostle).
     * @param bays New bay list. Defaults to empty (full reset).
     */
    void resetLayout(const std::vector<Bay>& bays = {}) {
        best_.bays = bays;
        grid_.clearAll();
        for (int i = 0; i < static_cast<int>(best_.bays.size()); ++i)
            grid_.insertBay(i, CollisionChecker::createSolidOBB(best_.bays[i], &info_));
    }

    /**
     * @brief Update best_ if the given solution scores lower.
     * Call this whenever a candidate layout finishes to track the global best.
     * @param candidate  A fully built Solution (bays + score already computed).
     */
    void updateBest(Solution candidate) {
        candidate.producedBy = name();
        candidate.timeTaken = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime_).count();
        if (candidate.score < best_.score)
            best_ = std::move(candidate);
    }

    // -------------------------------------------------------------------------
    // Protected members accessible to subclasses
    // -------------------------------------------------------------------------

    const StaticState& info_;  ///< shared, read-only problem data
    std::mt19937_64    rng_;   ///< per-thread RNG — seed in constructor
    SpatialGrid        grid_;  ///< broad-phase grid for the current layout
    Solution           best_;  ///< best solution found so far
    std::chrono::steady_clock::time_point startTime_; ///< timestamp when algorithm was constructed

private:
    /** Returns the largest bay footprint dimension to use as grid cell size. */
    static double largestBayDim(const StaticState& info) {
        double d = 1.0;
        for (const auto& bt : info.bayTypes)
            d = std::max(d, std::max(bt.width, bt.depth));
        return d;
    }
};
