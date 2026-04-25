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

    void calculateMetrics(Solution& sol) const {
        sol.training_score = computeTrainingScore(sol.bays, info_, warehouse_area_);
        sol.official_score = computeOfficialScore(sol.bays, info_, warehouse_area_);
    }

    double evaluateTraining(const std::vector<Bay>& bays) const {
        return computeTrainingScore(bays, info_, warehouse_area_);
    }

    // Safely blanket the warehouse with a grid of anchors to prevent corner-starvation
    std::vector<Point2D> generateSafeAnchors(double step_size = 0.0) const {
        std::vector<Point2D> anchors;
        if (info_.warehousePolygon.empty()) return anchors;
        
        // Dynamically calculate step size if not provided
        if (step_size <= 0.0) {
            step_size = 1e9;
            for (const auto& bt : info_.bayTypes) {
                step_size = std::min({step_size, bt.width, bt.depth});
            }
            step_size /= 2.0; // Half the smallest dimension guarantees no missed gaps
            if (step_size < 100.0) step_size = 100.0; // Sane lower bound
        }
        
        double bounds_min_x = info_.warehousePolygon[0].x, bounds_max_x = bounds_min_x;
        double bounds_min_y = info_.warehousePolygon[0].y, bounds_max_y = bounds_min_y;
        for (const auto& pt : info_.warehousePolygon) {
            bounds_min_x = std::min(bounds_min_x, pt.x); bounds_max_x = std::max(bounds_max_x, pt.x);
            bounds_min_y = std::min(bounds_min_y, pt.y); bounds_max_y = std::max(bounds_max_y, pt.y);
        }
        
        for (double x = bounds_min_x; x <= bounds_max_x; x += step_size) {
            for (double y = bounds_min_y; y <= bounds_max_y; y += step_size) {
                anchors.push_back({x + 0.1, y + 0.1});
            }
        }
        return anchors;
    }

    void addBayAnchors(const Bay& bay, std::vector<Point2D>& anchors) const {
        OBB solid = CollisionChecker::createSolidOBB(bay, &info_);
        OBB gap = CollisionChecker::createGapOBB(bay, &info_);
        for (int i = 0; i < 4; ++i) {
            anchors.push_back(solid.corners[i]);
            anchors.push_back(gap.corners[i]);
        }

        const BayType* type = CollisionChecker::getBayType(bay.typeId, &info_);
        if (!type) return;

        double w = type->width;
        double d = type->depth;
        double g = type->gap;
        double M_PI_VAL = 3.14159265358979323846;
        double cosA = std::cos(bay.rotation * M_PI_VAL / 180.0);
        double sinA = std::sin(bay.rotation * M_PI_VAL / 180.0);

        double wx1 = bay.x - (d+g)*sinA;
        double wy1 = bay.y + (d+g)*cosA;
        double wx2 = bay.x + w*cosA - (d+g)*sinA;
        double wy2 = bay.y + w*sinA + (d+g)*cosA;

        double nx = -sinA;
        double ny = cosA;

        for (const auto& bt : info_.bayTypes) {
            anchors.push_back({wx1 + nx * bt.depth, wy1 + ny * bt.depth});
            anchors.push_back({wx2 + nx * bt.depth, wy2 + ny * bt.depth});
        }
    }

    bool tryPlace(const Bay& bay) {
        if (!CollisionChecker::isValidPlacement(bay, best_.bays, &info_, &grid_)) return false;
        grid_.insertBay(static_cast<int>(best_.bays.size()), CollisionChecker::createBoundingOBB(bay, &info_));
        best_.bays.push_back(bay);
        return true;
    }

    void resetLayout(const std::vector<Bay>& bays = {}) {
        best_.bays = bays;
        grid_.clearAll();
        for (int i = 0; i < static_cast<int>(best_.bays.size()); ++i)
            grid_.insertBay(i, CollisionChecker::createBoundingOBB(best_.bays[i], &info_));
    }

    void updateBest(Solution candidate) {
        candidate.producedBy = name();
        candidate.timeTaken = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime_).count();
        
        // Algorithms optimize using the TRAINING score (Q_new)
        if (candidate.training_score < best_.training_score && !candidate.bays.empty()) {
            best_ = std::move(candidate);
        }
    }
};