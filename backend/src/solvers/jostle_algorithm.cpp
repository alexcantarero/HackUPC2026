#include "jostle_algorithm.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <thread>

JostleAlgorithm::JostleAlgorithm(const StaticState& info, uint64_t seed, int maxIterations)
    : Algorithm(info, seed), maxIterations_(maxIterations) {
}

std::string JostleAlgorithm::name() const {
    return "jostle";
}

bool JostleAlgorithm::isValidState(const std::vector<Bay>& state) {
    std::vector<Bay> placed;
    placed.reserve(state.size());
    for (const Bay& bay : state) {
        if (!CollisionChecker::isValidPlacement(bay, placed, &info_, nullptr)) return false;
        placed.push_back(bay);
    }
    return true;
}

bool JostleAlgorithm::isBayValidInState(const std::vector<Bay>& state, int bayIndex) {
    if (bayIndex < 0 || bayIndex >= (int)state.size()) return false;
    const Bay& candidate = state[bayIndex];
    std::vector<Bay> others;
    others.reserve(state.size());
    for (int i = 0; i < (int)state.size(); ++i) {
        if (i != bayIndex) others.push_back(state[i]);
    }
    return CollisionChecker::isValidPlacement(candidate, others, &info_, nullptr);
}

void JostleAlgorithm::greedyPlacement(std::vector<Bay>& state) {
    if (info_.bayTypes.empty() || info_.warehousePolygon.empty()) return;
    double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
    for(auto p : info_.warehousePolygon) {
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
    }
    std::vector<BayType> sortedTypes = info_.bayTypes;
    // Sort by "Efficiency": Area per Price. We want the most area for the least money.
    std::sort(sortedTypes.begin(), sortedTypes.end(), [](const BayType& a, const BayType& b){
        return (a.width * a.depth / a.price) > (b.width * b.depth / b.price);
    });
    
    // Fine-grained grid search for the seed
    for (double y = minY; y <= maxY; y += 100.0) {
        for (double x = minX; x <= maxX; x += 100.0) {
            for (const auto& bt : sortedTypes) {
                for (double rot : {0.0, 90.0, 180.0, 270.0}) {
                    Bay b = {bt.id, x, y, rot};
                    if (CollisionChecker::isValidPlacement(b, state, &info_, nullptr)) {
                        state.push_back(b);
                        goto next_pos;
                    }
                }
            }
            next_pos:;
        }
    }
}

bool JostleAlgorithm::translateBay(std::vector<Bay>& state, int bayIndex, double deltaX, double deltaY) {
    double oldX = state[bayIndex].x, oldY = state[bayIndex].y;
    state[bayIndex].x += deltaX; state[bayIndex].y += deltaY;
    if (isBayValidInState(state, bayIndex)) return true;
    state[bayIndex].x = oldX; state[bayIndex].y = oldY;
    return false;
}

bool JostleAlgorithm::rotateBay(std::vector<Bay>& state, int bayIndex, double deltaAngle) {
    double oldRot = state[bayIndex].rotation;
    state[bayIndex].rotation = std::fmod(oldRot + deltaAngle, 360.0);
    if (state[bayIndex].rotation < 0) state[bayIndex].rotation += 360.0;
    if (isBayValidInState(state, bayIndex)) return true;
    state[bayIndex].rotation = oldRot;
    return false;
}

bool JostleAlgorithm::swapBays(std::vector<Bay>& state, int bayIndex1, int bayIndex2) {
    if (bayIndex1 == bayIndex2) return true;
    std::swap(state[bayIndex1].typeId, state[bayIndex2].typeId);
    if (isBayValidInState(state, bayIndex1) && isBayValidInState(state, bayIndex2)) return true;
    std::swap(state[bayIndex1].typeId, state[bayIndex2].typeId);
    return false;
}

bool JostleAlgorithm::changeBayType(std::vector<Bay>& state, int bayIndex, int newTypeId) {
    int oldId = state[bayIndex].typeId;
    state[bayIndex].typeId = newTypeId;
    if (isBayValidInState(state, bayIndex)) return true;
    state[bayIndex].typeId = oldId;
    return false;
}

bool JostleAlgorithm::addRandomBay(std::vector<Bay>& state) {
    if (info_.bayTypes.empty() || info_.warehousePolygon.empty()) return false;
    double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
    for(auto p : info_.warehousePolygon) {
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
    }
    std::uniform_int_distribution<int> type_dist(0, (int)info_.bayTypes.size() - 1);
    std::uniform_real_distribution<double> x_dist(minX, maxX);
    std::uniform_real_distribution<double> y_dist(minY, maxY);
    std::uniform_real_distribution<double> rot_dist(0.0, 360.0);
    std::uniform_real_distribution<double> prob(0.0, 1.0);

    for (int attempt = 0; attempt < 50; ++attempt) {
        double rx, ry;
        if (!state.empty() && prob(rng_) < 0.90) {
            int ref = std::uniform_int_distribution<int>(0, state.size() - 1)(rng_);
            double range = 1500.0;
            std::uniform_real_distribution<double> offset(-range, range);
            rx = state[ref].x + offset(rng_); ry = state[ref].y + offset(rng_);
        } else {
            rx = x_dist(rng_); ry = y_dist(rng_);
        }
        Bay newBay = {info_.bayTypes[type_dist(rng_)].id, rx, ry, rot_dist(rng_)};
        if (CollisionChecker::isValidPlacement(newBay, state, &info_, nullptr)) {
            state.push_back(newBay); return true;
        }
    }
    return false;
}

bool JostleAlgorithm::removeRandomBay(std::vector<Bay>& state) {
    if (state.empty()) return false;
    std::uniform_int_distribution<int> dist(0, (int)state.size() - 1);
    state.erase(state.begin() + dist(rng_));
    return true;
}

void JostleAlgorithm::run(std::atomic<bool>& stop_flag) {
    std::vector<Bay> current_state;
    greedyPlacement(current_state);
    
    // GOAL: Maximize Area, minimize Price.
    // We compute a score that gets SMALLER as we improve (for updateBest compatibility).
    auto getJostleScore = [&](const std::vector<Bay>& s) {
        if (s.empty()) return 1e18;
        double area_sum = 0;
        double cost_sum = 0;
        for (const auto& b : s) {
            const auto* bt = CollisionChecker::getBayType(b.typeId, &info_);
            if (bt) {
                area_sum += bt->width * bt->depth;
                cost_sum += bt->price / bt->nLoads;
            }
        }
        // Score = Price / (Area^2). 
        // Denominator grows much faster than numerator when adding bays.
        // Resulting score decreases as we add bays (correct for minimization).
        return cost_sum / (area_sum * area_sum + 1.0);
    };

    double current_score = getJostleScore(current_state);
    if (!current_state.empty()) {
        updateBest({current_state, current_score, name(), 0.0});
    }

    std::uniform_real_distribution<double> move_dist(-50.0, 50.0); 
    std::uniform_real_distribution<double> rot_delta_dist(-15.0, 15.0); 
    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
    
    // Aggressively prioritize Adding (weight 80) to fill the warehouse
    std::discrete_distribution<int> op_dist({80, 1, 10, 5, 2, 2}); 

    int iterations = 0;
    double temp = 1.0; 
    const double cooling = 0.999995;

    while (!stop_flag) {
        if (maxIterations_ > 0 && iterations >= maxIterations_) break;
        
        int op = op_dist(rng_);
        bool success = false;
        int mod_idx1 = -1, mod_idx2 = -1;
        Bay backup_bay1, backup_bay2;
        bool added = false, removed = false;

        if (op == 0) {
            if (addRandomBay(current_state)) { success = true; added = true; }
        } else if (op == 1 && current_state.size() > 5) { // Protect small layouts from reduction
            mod_idx1 = std::uniform_int_distribution<int>(0, current_state.size() - 1)(rng_);
            backup_bay1 = current_state[mod_idx1];
            current_state.erase(current_state.begin() + mod_idx1);
            removed = true; success = true;
        } else if (!current_state.empty()) {
            mod_idx1 = std::uniform_int_distribution<int>(0, current_state.size() - 1)(rng_);
            backup_bay1 = current_state[mod_idx1];
            if (op == 2) success = translateBay(current_state, mod_idx1, move_dist(rng_), move_dist(rng_));
            else if (op == 3) success = rotateBay(current_state, mod_idx1, rot_delta_dist(rng_));
            else if (op == 4) {
                mod_idx2 = (mod_idx1 + 1) % (int)current_state.size();
                backup_bay2 = current_state[mod_idx2];
                success = swapBays(current_state, mod_idx1, mod_idx2);
            } else if (op == 5) {
                int tid = info_.bayTypes[std::uniform_int_distribution<int>(0, info_.bayTypes.size()-1)(rng_)].id;
                success = changeBayType(current_state, mod_idx1, tid);
            }
        }

        if (success) {
            double next_score = getJostleScore(current_state);
            double delta = next_score - current_score;
            
            // Optimization logic: Minimize our internal efficiency-based score
            if (delta <= 0 || (temp > 1e-6 && prob_dist(rng_) < std::exp(-delta / (current_score * temp)))) {
                current_score = next_score;
                if (delta < 0) {
                    updateBest({current_state, current_score, name(), 0.0});
                }
            } else {
                // Revert
                if (added) current_state.pop_back();
                else if (removed) current_state.insert(current_state.begin() + mod_idx1, backup_bay1);
                else {
                    current_state[mod_idx1] = backup_bay1;
                    if (mod_idx2 != -1) current_state[mod_idx2] = backup_bay2;
                }
            }
        }

        temp *= cooling;
        if (temp < 0.1) temp = 1.0; 
        iterations++;
        if (iterations % 500 == 0) std::this_thread::yield();
    }
}
