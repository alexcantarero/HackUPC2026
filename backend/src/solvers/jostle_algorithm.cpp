#include "jostle_algorithm.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

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
        if (!CollisionChecker::isValidPlacement(bay, placed, &info_, nullptr)) {
            return false;
        }
        placed.push_back(bay);
    }
    return true;
}

bool JostleAlgorithm::isBayValidInState(const std::vector<Bay>& state, int bayIndex) {
    if (bayIndex < 0 || bayIndex >= (int)state.size()) return false;
    
    thread_local std::vector<Bay> others;
    others.clear();
    others.reserve(state.size());
    for (int i = 0; i < (int)state.size(); ++i) {
        if (i != bayIndex) {
            others.push_back(state[i]);
        }
    }

    return CollisionChecker::isValidPlacement(state[bayIndex], others, &info_, nullptr);
}

void JostleAlgorithm::greedyPlacement(std::vector<Bay>& state) {
    if (info_.bayTypes.empty() || info_.warehousePolygon.empty()) return;

    double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
    for(auto p : info_.warehousePolygon) {
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
    }
    if (minX >= maxX || minY >= maxY) return;

    std::vector<BayType> sortedTypes = info_.bayTypes;
    std::sort(sortedTypes.begin(), sortedTypes.end(), [](const BayType& a, const BayType& b){
        return (a.width * a.depth * a.nLoads) > (b.width * b.depth * b.nLoads);
    });

    // Proportional step sizes to guarantee exactly 10,000 grid evaluations (lightning fast)
    // regardless of whether coordinates are in meters or millimeters.
    double stepX = (maxX - minX) / 100.0;
    double stepY = (maxY - minY) / 100.0;
    if (stepX <= 0.1) stepX = 1.0;
    if (stepY <= 0.1) stepY = 1.0;

    for (double y = minY; y <= maxY; y += stepY) {
        for (double x = minX; x <= maxX; x += stepX) {
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
    state[bayIndex].x += deltaX;
    state[bayIndex].y += deltaY;
    if (isBayValidInState(state, bayIndex)) return true;
    state[bayIndex].x = oldX; state[bayIndex].y = oldY;
    return false;
}

bool JostleAlgorithm::rotateBay(std::vector<Bay>& state, int bayIndex, double /*deltaAngle*/) {
    double oldRot = state[bayIndex].rotation;
    state[bayIndex].rotation = std::fmod(oldRot + 90.0, 360.0);
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
    if (minX >= maxX) return false;

    std::uniform_int_distribution<int> type_dist(0, (int)info_.bayTypes.size() - 1);
    std::uniform_real_distribution<double> x_dist(minX, maxX);
    std::uniform_real_distribution<double> y_dist(minY, maxY);
    
    const double angles[] = {0.0, 90.0, 180.0, 270.0};
    std::uniform_int_distribution<int> angle_dist(0, 3);
    std::uniform_real_distribution<double> prob(0.0, 1.0);

    // 30 attempts per SA tick to successfully spawn a bay
    for (int attempt = 0; attempt < 30; ++attempt) {
        double rx, ry;
        
        // 80% chance to organically cluster around existing bays
        if (!state.empty() && prob(rng_) < 0.80) {
            std::uniform_int_distribution<int> bay_dist(0, (int)state.size() - 1);
            int ref = bay_dist(rng_);
            double offset_range = largestBayDim(info_) * 1.5; 
            std::uniform_real_distribution<double> offset(-offset_range, offset_range);
            rx = state[ref].x + offset(rng_);
            ry = state[ref].y + offset(rng_);
        } else {
            rx = x_dist(rng_);
            ry = y_dist(rng_);
        }

        Bay newBay = {info_.bayTypes[type_dist(rng_)].id, rx, ry, angles[angle_dist(rng_)]};
        
        if (CollisionChecker::isValidPlacement(newBay, state, &info_, nullptr)) {
            state.push_back(newBay);
            return true;
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
    
    double current_score = calculateScore(current_state);
    
    bool is_minimization = true; 
    
    // Explicitly update best with the greedy seed
    if (!current_state.empty()) {
        Solution seed;
        seed.bays = current_state;
        seed.score = current_score;
        updateBest(seed);
    }

    std::uniform_real_distribution<double> move_dist(-20.0, 20.0); 
    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
    
    // Aggressively prioritize adding bays (weight 50) over removing (weight 5)
    std::discrete_distribution<int> op_dist({50, 5, 20, 15, 5, 5}); 

    int iterations = 0;
    double temp = 1000.0;
    const double cooling = 0.9999;

    while (!stop_flag && (maxIterations_ <= 0 || iterations < maxIterations_)) {
        int op = op_dist(rng_);
        bool success = false;
        
        int mod_idx1 = -1, mod_idx2 = -1;
        Bay backup_bay1, backup_bay2;
        bool added = false, removed = false;

        if (op == 0) {
            success = addRandomBay(current_state);
            if (success) added = true;
        } else if (op == 1) {
            if (!current_state.empty()) {
                std::uniform_int_distribution<int> dist(0, (int)current_state.size() - 1);
                mod_idx1 = dist(rng_);
                backup_bay1 = current_state[mod_idx1];
                current_state.erase(current_state.begin() + mod_idx1);
                removed = true;
                success = true;
            }
        } else if (!current_state.empty()) {
            std::uniform_int_distribution<int> bay_dist(0, (int)current_state.size() - 1);
            mod_idx1 = bay_dist(rng_);
            backup_bay1 = current_state[mod_idx1];
            
            if (op == 2) {
                success = translateBay(current_state, mod_idx1, move_dist(rng_), move_dist(rng_));
            } else if (op == 3) {
                success = rotateBay(current_state, mod_idx1, 90.0);
            } else if (op == 4) {
                mod_idx2 = (mod_idx1 + 1) % (int)current_state.size();
                backup_bay2 = current_state[mod_idx2];
                success = swapBays(current_state, mod_idx1, mod_idx2);
            } else if (op == 5) {
                std::uniform_int_distribution<int> type_dist(0, (int)info_.bayTypes.size() - 1);
                success = changeBayType(current_state, mod_idx1, info_.bayTypes[type_dist(rng_)].id);
            }
        }

        if (success) {
            double next_score = calculateScore(current_state);
            
            // Standardize the delta check so SA logic works perfectly either way
            double delta;
            if (is_minimization) {
                delta = next_score - current_score;
            } else {
                delta = current_score - next_score;
            }
            
            if (delta <= 0 || prob_dist(rng_) < std::exp(-delta / temp)) {
                current_score = next_score;
                
                // If it's a strict improvement over the current step, blast it to the base class
                if (delta <= 0) {
                    updateBest({current_state, current_score, name(), 0.0});
                }
            } else {
                if (added) {
                    current_state.pop_back();
                } else if (removed) {
                    current_state.insert(current_state.begin() + mod_idx1, backup_bay1);
                } else {
                    current_state[mod_idx1] = backup_bay1;
                    if (mod_idx2 != -1) current_state[mod_idx2] = backup_bay2;
                }
            }
        }

        temp *= cooling;
        if (temp < 0.1) temp = 1000.0;
        iterations++;
    }
}