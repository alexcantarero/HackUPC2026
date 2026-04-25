#include "sa.hpp"
#include "../core/collision.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

// ── Constructor ───────────────────────────────────────────────────────────────

SimulatedAnnealing::SimulatedAnnealing(const StaticState& info, uint64_t seed)
    : Algorithm(info, seed)
    , wh_area_(warehouseArea(info.warehousePolygon))
    , sum_ratio_(0.0)
    , sum_area_(0.0)
    , current_grid_(largestBayDim(info))
{}

// ── run() ─────────────────────────────────────────────────────────────────────

void SimulatedAnnealing::run(std::atomic<bool>& stop_flag) {
    // Phase 1: greedy warm start
    GreedySolver greedy(info_, rng_());
    greedy.fillPass();
    initFromBays(greedy.best().bays);

    // Commit greedy result as the first known best
    updateBest(greedy.best());

    if (current_bays_.empty()) return;

    // Phase 2: SA
    double T = calibrateT0();
    constexpr double ALPHA = 0.9995;

    std::uniform_int_distribution<int> move_dist(0, 1);

    while (!stop_flag) {
        bool accepted = (move_dist(rng_) == 0)
                        ? moveRelocate(T)
                        : moveReplaceType(T);

        if (accepted) {
            Solution currentSol;
            currentSol.bays   = current_bays_;
            currentSol.score  = currentScore();
            updateBest(currentSol);
        }

        T *= ALPHA;
    }
}

// ── State management ────────────────────────────────────────────────────────

double SimulatedAnnealing::currentScore() const {
    return sum_ratio_ * sum_ratio_ - sum_area_ / wh_area_;
}

void SimulatedAnnealing::initFromBays(const std::vector<Bay>& bays) {
    current_bays_ = bays;
    current_grid_.clearAll();
    for (int i = 0; i < static_cast<int>(current_bays_.size()); ++i)
        current_grid_.insertBay(i, CollisionChecker::createSolidOBB(current_bays_[i], &info_));

    sum_ratio_ = 0.0;
    sum_area_  = 0.0;
    for (const auto& bay : current_bays_) {
        sum_ratio_ += bayRatio(bay.typeId);
        sum_area_  += bayArea(bay.typeId);
    }
    rebuildEventPoints();
}

void SimulatedAnnealing::rebuildEventPoints() {
    event_points_.clear();
    for (const auto& bay : current_bays_) {
        OBB solid = CollisionChecker::createSolidOBB(bay, &info_);
        for (int i = 0; i < 4; ++i)
            event_points_.push_back(solid.corners[i]);
    }
    if (event_points_.empty() && !info_.warehousePolygon.empty()) {
        double min_x = info_.warehousePolygon[0].x;
        double min_y = info_.warehousePolygon[0].y;
        for (const auto& p : info_.warehousePolygon) {
            if (p.x < min_x) min_x = p.x;
            if (p.y < min_y) min_y = p.y;
        }
        event_points_.push_back({min_x, min_y});
    }
}

Bay SimulatedAnnealing::removeBayAt(int idx) {
    Bay removed = current_bays_[idx];
    current_bays_.erase(current_bays_.begin() + idx);
    current_grid_.clearBays();
    for (int i = 0; i < static_cast<int>(current_bays_.size()); ++i)
        current_grid_.insertBay(i, CollisionChecker::createSolidOBB(current_bays_[i], &info_));
    return removed;
}

void SimulatedAnnealing::appendBay(const Bay& bay) {
    current_grid_.insertBay(static_cast<int>(current_bays_.size()),
                            CollisionChecker::createSolidOBB(bay, &info_));
    current_bays_.push_back(bay);
}

// ── Calibration ───────────────────────────────────────────────────────────────

double SimulatedAnnealing::calibrateT0() {
    if (current_bays_.empty() || info_.bayTypes.size() < 2)
        return 1.0;

    std::uniform_int_distribution<int> bay_dist(
        0, static_cast<int>(current_bays_.size()) - 1);
    std::uniform_int_distribution<int> type_dist(
        0, static_cast<int>(info_.bayTypes.size()) - 1);

    double sum_delta = 0.0;
    int    count     = 0;

    for (int probe = 0; probe < 200; ++probe) {
        const Bay& b        = current_bays_[bay_dist(rng_)];
        int        new_tidx = type_dist(rng_);
        const BayType& nt   = info_.bayTypes[new_tidx];
        if (nt.id == b.typeId) continue;

        double new_sum_r = sum_ratio_ - bayRatio(b.typeId) + nt.price / nt.nLoads;
        double new_sum_a = sum_area_  - bayArea(b.typeId)  + nt.width * nt.depth;
        double delta = std::fabs(new_sum_r * new_sum_r - new_sum_a / wh_area_
                                 - currentScore());
        sum_delta += delta;
        ++count;
    }

    if (count == 0 || sum_delta == 0.0) return 1.0;
    return (sum_delta / count) / std::log(2.0);
}

// ── Move: Relocate ────────────────────────────────────────────────────────

bool SimulatedAnnealing::moveRelocate(double T) {
    (void)T; // ΔQ = 0 for same-type relocation → always accept if valid
    if (current_bays_.empty() || event_points_.empty()) return false;

    std::uniform_int_distribution<int> bay_dist(
        0, static_cast<int>(current_bays_.size()) - 1);
    std::uniform_int_distribution<int> ep_dist(
        0, static_cast<int>(event_points_.size()) - 1);

    int  idx     = bay_dist(rng_);
    Bay  removed = removeBayAt(idx);

    Point2D new_pos = event_points_[ep_dist(rng_)];

    std::array<double, 13> rots;
    for (int i = 0; i < 13; ++i) rots[i] = i * 15.0;
    std::shuffle(rots.begin(), rots.end(), rng_);

    for (double rot : rots) {
        Bay candidate{removed.typeId, new_pos.x, new_pos.y, rot};
        if (CollisionChecker::isValidPlacement(
                candidate, current_bays_, &info_, &current_grid_)) {
            appendBay(candidate);
            // sums unchanged (same type)
            rebuildEventPoints();
            return true;
        }
    }

    // Reject: restore removed bay at end (order doesn't affect correctness)
    appendBay(removed);
    return false;
}

// ── Move: Replace Type ────────────────────────────────────────────────────────

bool SimulatedAnnealing::moveReplaceType(double T) {
    if (current_bays_.empty() || info_.bayTypes.size() < 2) return false;

    std::uniform_int_distribution<int> bay_dist(
        0, static_cast<int>(current_bays_.size()) - 1);
    std::uniform_int_distribution<int> type_dist(
        0, static_cast<int>(info_.bayTypes.size()) - 1);
    std::uniform_real_distribution<double> unif(0.0, 1.0);

    int idx            = bay_dist(rng_);
    const Bay& current = current_bays_[idx];
    double old_ratio   = bayRatio(current.typeId);
    double old_area    = bayArea(current.typeId);

    // Pick a different bay type
    int new_tidx;
    do { new_tidx = type_dist(rng_); }
    while (info_.bayTypes[new_tidx].id == current.typeId);

    const BayType& nt = info_.bayTypes[new_tidx];
    double new_ratio  = nt.price / nt.nLoads;
    double new_area   = nt.width * nt.depth;

    // Early Metropolis check (skip expensive SAT if thermodynamically rejected)
    double new_sum_r  = sum_ratio_ - old_ratio + new_ratio;
    double new_sum_a  = sum_area_  - old_area  + new_area;
    double new_score  = new_sum_r * new_sum_r - new_sum_a / wh_area_;
    double delta      = new_score - currentScore();

    if (delta >= 0.0) {
        if (T <= 0.0 || unif(rng_) >= std::exp(-delta / T))
            return false; // rejected before attempting placement
    }

    // Attempt placement: remove current bay, try new type at same position
    Bay removed = removeBayAt(idx);

    std::array<double, 13> rots;
    for (int i = 0; i < 13; ++i) rots[i] = i * 15.0;
    std::shuffle(rots.begin(), rots.end(), rng_);

    for (double rot : rots) {
        Bay candidate{nt.id, removed.x, removed.y, rot};
        if (CollisionChecker::isValidPlacement(
                candidate, current_bays_, &info_, &current_grid_)) {
            appendBay(candidate);
            sum_ratio_ = new_sum_r;
            sum_area_  = new_sum_a;
            rebuildEventPoints();
            return true;
        }
    }

    // Reject: restore original bay
    appendBay(removed);
    return false;
}

// ── Incremental helpers ────────────────────────────────────────────────────────

double SimulatedAnnealing::bayRatio(int typeId) const {
    for (const auto& bt : info_.bayTypes)
        if (bt.id == typeId) return bt.price / bt.nLoads;
    return 0.0;
}

double SimulatedAnnealing::bayArea(int typeId) const {
    for (const auto& bt : info_.bayTypes)
        if (bt.id == typeId) return bt.width * bt.depth;
    return 0.0;
}

double SimulatedAnnealing::largestBayDim(const StaticState& info) {
    double d = 1.0;
    for (const auto& bt : info.bayTypes)
        d = std::max(d, std::max(bt.width, bt.depth));
    return d;
}