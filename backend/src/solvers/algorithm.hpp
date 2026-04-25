#pragma once

#include "../core/types.hpp"
#include "../core/collision.hpp"
#include "../core/score.hpp" // Added for scoring
#include <atomic>
#include <random>
#include <string>
#include <algorithm>
#include <chrono>

class Algorithm {
public:
    Algorithm(const StaticState& info, uint64_t seed)
        : info_(info)
        , rng_(seed)
        , grid_(largestBayDim(info))
        , warehouse_area_(warehouseArea(info.warehousePolygon)) // Cache area once!
        , startTime_(std::chrono::steady_clock::now())
    {}

    virtual ~Algorithm() = default;
    Algorithm(const Algorithm&)            = delete;
    Algorithm& operator=(const Algorithm&) = delete;

    virtual void run(std::atomic<bool>& stop_flag) = 0;
    virtual std::string name() const = 0;
    const Solution& best() const { return best_; }

protected:
    const StaticState& info_;  
    std::mt19937_64    rng_;   
    SpatialGrid        grid_;  
    double             warehouse_area_; // Stored here for easy access
    Solution           best_;  
    std::chrono::steady_clock::time_point startTime_; 

    // ALL solvers now use this exact same scoring method!
    double calculateScore(const std::vector<Bay>& bays) const {
        return computeScore(bays, info_, warehouse_area_);
    }

    bool tryPlace(const Bay& bay) {
        if (!CollisionChecker::isValidPlacement(bay, best_.bays, &info_, &grid_)) return false;
        grid_.insertBay(static_cast<int>(best_.bays.size()), CollisionChecker::createSolidOBB(bay, &info_));
        best_.bays.push_back(bay);
        return true;
    }

    void resetLayout(const std::vector<Bay>& bays = {}) {
        best_.bays = bays;
        grid_.clearAll();
        for (int i = 0; i < static_cast<int>(best_.bays.size()); ++i)
            grid_.insertBay(i, CollisionChecker::createSolidOBB(best_.bays[i], &info_));
    }

    void updateBest(Solution candidate) {
        candidate.producedBy = name();
        candidate.timeTaken = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime_).count();
        if (candidate.score < best_.score) best_ = std::move(candidate);
    }

private:
    static double largestBayDim(const StaticState& info) {
        double d = 1.0;
        for (const auto& bt : info.bayTypes) d = std::max(d, std::max(bt.width, bt.depth));
        return d;
    }
};