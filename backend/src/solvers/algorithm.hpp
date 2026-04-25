#pragma once

#include "../core/types.hpp"
#include "../core/collision.hpp"
#include "../core/score.hpp"
#include <atomic>
#include <random>
#include <string>
#include <algorithm>
#include <chrono>
#include <iostream>

class Algorithm {
public:
    Algorithm(const StaticState& info, uint64_t seed)
        : info_(info)
        , rng_(seed)
        , grid_(largestBayDim(info))
        , warehouse_area_(warehouseArea(info.warehousePolygon))
        , startTime_(std::chrono::steady_clock::now())
    {}

    virtual ~Algorithm() = default;
    Algorithm(const Algorithm&)            = delete;
    Algorithm& operator=(const Algorithm&) = delete;

    virtual void run(std::atomic<bool>& stop_flag) = 0;
    virtual std::string name() const = 0;
    virtual bool isAnytime() const { return true; }
    const Solution& best() const { return best_; }

protected:
    const StaticState& info_;  
    std::mt19937_64    rng_;   
    SpatialGrid        grid_;  
    double             warehouse_area_;
    Solution           best_;  
    std::chrono::steady_clock::time_point startTime_; 

    static double largestBayDim(const StaticState& info) {
        double d = 1.0;
        for (const auto& bt : info.bayTypes) d = std::max(d, std::max(bt.width, bt.depth));
        return d;
    }

    double calculateScore(const std::vector<Bay>& bays) const {
        return computeScore(bays, info_, warehouse_area_);
    }

    // Safely blanket the warehouse with a grid of anchors to prevent corner-starvation
    std::vector<Point2D> generateSafeAnchors() const {
        std::vector<Point2D> anchors;
        if (info_.warehousePolygon.empty()) return anchors;
        
        double min_x = info_.warehousePolygon[0].x, max_x = min_x;
        double min_y = info_.warehousePolygon[0].y, max_y = min_y;
        for (const auto& pt : info_.warehousePolygon) {
            min_x = std::min(min_x, pt.x); max_x = std::max(max_x, pt.x);
            min_y = std::min(min_y, pt.y); max_y = std::max(max_y, pt.y);
        }
        
        // A 400x400 grid. The +0.1 prevents ray-casting from failing on perfect boundaries.
        for (double x = min_x; x <= max_x; x += 400.0) {
            for (double y = min_y; y <= max_y; y += 400.0) {
                anchors.push_back({x + 0.1, y + 0.1});
            }
        }
        return anchors;
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
        if (candidate.score < best_.score) {
            best_ = std::move(candidate);
        }
    }
};