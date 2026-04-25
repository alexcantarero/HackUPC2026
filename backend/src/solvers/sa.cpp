#include "sa.hpp"
#include "../core/collision.hpp"
#include <algorithm>    
#include <array>
#include <cmath>
#include "../core/score.hpp"
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
/*
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

    int epoch_length = current_bays_.size() * 20; // Example: Try 20 moves per bay before cooling

    while (!stop_flag) {
        for (int step = 0; step < epoch_length && !stop_flag; ++step) {
            bool accepted = (move_dist(rng_) == 0)
                            ? moveRelocate(T)
                            : moveReplaceType(T);

            if (accepted) {
                Solution currentSol;
                currentSol.bays = current_bays_;
                calculateMetrics(currentSol);
                updateBest(currentSol);
            }
        }
        // Only cool down AFTER the epoch finishes
        T *= ALPHA; 
    }

}
*/

// run() with dynamic temperature based on elapsed time to ensure we respect the 30s limit without relying on a fixed ALPHA
/*
void SimulatedAnnealing::run(std::atomic<bool>& stop_flag) {
    // Phase 1: greedy warm start
    GreedySolver greedy(info_, rng_());
    greedy.fillPass();
    initFromBays(greedy.best().bays);
    updateBest(greedy.best());

    if (current_bays_.empty()) return;

    // Phase 2: SA Setup
    const double T0 = calibrateT0();
    
    // Set your time budget slightly under the 29s limit to ensure safe exit
    const double MAX_TIME_SEC = 28.9; 
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Epoch length: how many moves to try before checking the clock and cooling.
    // Adjust this so you check the clock frequently enough, but not so often 
    // that chrono slows down your loop. 500-2000 is usually a sweet spot.
    const int EPOCH_LENGTH = 1000; 

    std::uniform_int_distribution<int> move_dist(0, 1);

    while (!stop_flag) {
        // 1. Check the clock
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_chrono = now - start_time;
        double elapsed = elapsed_chrono.count();

        // 2. Safety break if we exceed our internal budget
        if (elapsed >= MAX_TIME_SEC) {
            break; 
        }

        // 3. Calculate dynamic temperature
        double progress = elapsed / MAX_TIME_SEC;
        // Using power of 3 for a smooth curve with a long fine-tuning tail
        double T = T0 * std::pow(1.0 - progress, 3.0); 

        // 4. Run the epoch at this temperature
        for (int step = 0; step < EPOCH_LENGTH && !stop_flag; ++step) {
            bool accepted = (move_dist(rng_) == 0)
                            ? moveRelocate(T)
                            : moveReplaceType(T);

            if (accepted) {
                Solution currentSol;
                currentSol.bays = current_bays_;
                calculateMetrics(currentSol);
                updateBest(currentSol);
            }
        }
    }
}
*/

void SimulatedAnnealing::run(std::atomic<bool>& stop_flag) {
    // Phase 1: greedy warm start
    GreedySolver greedy(info_, rng_());
    greedy.fillPass();
    initFromBays(greedy.best().bays);
    updateBest(greedy.best());

    if (current_bays_.empty()) return;

    // Phase 2: SA Setup
    const double T0 = calibrateT0();
    
    const double MAX_TIME_SEC = 28.9; 
    const int NUM_SEASONS = 3; 
    const double SEASON_TIME = MAX_TIME_SEC / NUM_SEASONS;
    
    auto start_time = std::chrono::steady_clock::now();
    const int EPOCH_LENGTH = 1000; 
    
    int current_season = 0;

    // 0 = Relocate, 1 = Replace Type, 2 = Jitter (Jitter unused for now, but can be added as a 3rd move type if desired, doesn't really work well)
    std::uniform_int_distribution<int> move_dist(0, 1);

    while (!stop_flag) {
        // 1. Check the clock
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - start_time).count();

        // 2. Safety break
        if (elapsed >= MAX_TIME_SEC) break; 

        // 3. Determine our current season
        int calc_season = static_cast<int>(elapsed / SEASON_TIME);

        // 4. REHEATING TRIGGER: Did we just cross into a new season?
        if (calc_season > current_season) {
            current_season = calc_season;
            
            // Teleport back to the best known solution to branch off in a new direction.
            // initFromBays does an O(N) grid rebuild, but doing it once every 9.6 seconds
            // has zero negative impact on performance.
            initFromBays(best_.bays); 
        }

        // 5. Calculate progress within the CURRENT season (0.0 to 1.0)
        double season_elapsed = elapsed - (current_season * SEASON_TIME);
        double progress = season_elapsed / SEASON_TIME;

        // 6. Calculate dynamic temperature for this season
        double T = T0 * std::pow(1.0 - progress, 3.0); 

        // 7. Run the epoch
        for (int step = 0; step < EPOCH_LENGTH && !stop_flag; ++step) {
            bool accepted = (move_dist(rng_) == 0)
                            ? moveRelocate(T)
                            : moveReplaceType(T);

            if (accepted) {
                Solution currentSol;
                currentSol.bays = current_bays_;
                calculateMetrics(currentSol);
                updateBest(currentSol);
            }
        }
    }
}

// ── State management ────────────────────────────────────────────────────────

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

/*
Bay SimulatedAnnealing::removeBayAt(int idx) {
    Bay removed = current_bays_[idx];
    current_bays_.erase(current_bays_.begin() + idx);
    current_grid_.clearBays();
    for (int i = 0; i < static_cast<int>(current_bays_.size()); ++i)
        current_grid_.insertBay(i, CollisionChecker::createSolidOBB(current_bays_[i], &info_));
    return removed;
}
*/

Bay SimulatedAnnealing::removeBayAt(int idx) {
    Bay removed = current_bays_[idx];
    int last_idx = static_cast<int>(current_bays_.size()) - 1;

    // 1. Remove the target bay from the spatial grid O(1)
    OBB removed_obb = CollisionChecker::createSolidOBB(removed, &info_);
    current_grid_.removeBay(idx, removed_obb);

    // 2. If the bay we are removing is NOT already the last element,
    // we need to perform a swap-and-pop to avoid an O(N) std::erase shift.
    if (idx != last_idx) {
        // Get the last bay
        Bay last_bay = current_bays_[last_idx];
        OBB last_obb = CollisionChecker::createSolidOBB(last_bay, &info_);

        // Remove the last bay from the grid at its OLD index
        current_grid_.removeBay(last_idx, last_obb);

        // Move the last bay into the vacated spot in the vector
        current_bays_[idx] = last_bay;

        // Re-insert the moved bay into the grid at its NEW index (idx)
        current_grid_.insertBay(idx, last_obb);
    }

    // 3. Pop the duplicated last element from the vector
    current_bays_.pop_back();

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
    double cur_score = evaluateTraining(current_bays_);

    for (int probe = 0; probe < 200; ++probe) {
        const Bay& b        = current_bays_[bay_dist(rng_)];
        int        new_tidx = type_dist(rng_);
        const BayType& nt   = info_.bayTypes[new_tidx];
        if (nt.id == b.typeId) continue;

        double new_sum_r = sum_ratio_ - bayRatio(b.typeId) + nt.price / nt.nLoads;
        double new_sum_a = sum_area_  - bayArea(b.typeId)  + nt.width * nt.depth;
        if (new_sum_a <= 0.0) continue;
        double af    = wh_area_ / new_sum_a;
        double new_s = new_sum_r * af * af;
        double delta = std::fabs(new_s - cur_score);
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

bool SimulatedAnnealing::moveJitter(double T) {
    (void)T; // ΔQ = 0 for spatial jitter -> thermodynamically always accepted
    if (current_bays_.empty()) return false;

    std::uniform_int_distribution<int> bay_dist(0, static_cast<int>(current_bays_.size()) - 1);
    
    // Tiny continuous adjustments: e.g., max 2.0 units of distance, max 3.0 degrees of rotation
    std::uniform_real_distribution<double> shift_dist(-2.0, 2.0); 
    std::uniform_real_distribution<double> rot_dist(-3.0, 3.0);   

    int idx = bay_dist(rng_);
    Bay removed = removeBayAt(idx);

    // Apply micro-adjustments
    Bay candidate = removed;
    candidate.x += shift_dist(rng_);
    candidate.y += shift_dist(rng_);
    candidate.rotation += rot_dist(rng_);

    // Normalize rotation if your engine requires it (e.g., 0 to 360)
    if (candidate.rotation < 0.0) candidate.rotation += 360.0;
    if (candidate.rotation >= 360.0) candidate.rotation -= 360.0;

    // Metropolis check: Jitter doesn't change area/ratio (since type is the same), 
    // so Delta Score is 0. We ALWAYS accept it if it's physically valid!
    if (CollisionChecker::isValidPlacement(candidate, current_bays_, &info_, &current_grid_)) {
        appendBay(candidate);
        // Note: rebuildEventPoints() is optional here depending on how strict you want to be,
        // but doing it keeps your event_points_ accurate for the macro moves.
        rebuildEventPoints(); 
        return true;
    }

    // Reject: restore original
    appendBay(removed);
    return false;
}

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
    double af         = (new_sum_a > 0.0) ? wh_area_ / new_sum_a : 0.0;
    double new_score  = new_sum_r * af * af;
    double delta      = new_score - evaluateTraining(current_bays_);

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