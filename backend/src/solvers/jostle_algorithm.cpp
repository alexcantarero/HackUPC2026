#include "jostle_algorithm.hpp"
#include <cmath>
#include <algorithm>

JostleAlgorithm::JostleAlgorithm(const StaticState& info, uint64_t seed, int maxIterations)
    : Algorithm(info, seed), maxIterations_(maxIterations) {
}

std::string JostleAlgorithm::name() const {
    return "jostle";
}

bool JostleAlgorithm::isValidState(const std::vector<Bay>& state) {
    std::vector<Bay> placed;
    placed.reserve(state.size());
    SpatialGrid testGrid(largestBayDim(info_)); 

    for (const Bay& bay : state) {
        if (!CollisionChecker::isValidPlacement(bay, placed, &info_, &testGrid)) {
            return false;
        }
        testGrid.insertBay(static_cast<int>(placed.size()), CollisionChecker::createSolidOBB(bay, &info_));
        placed.push_back(bay);
    }
    return true;
}

bool JostleAlgorithm::isBayValidInState(const std::vector<Bay>& state, int bayIndex) {
    if (bayIndex < 0 || bayIndex >= (int)state.size()) return false;
    
    const Bay& candidate = state[bayIndex];
    std::vector<Bay> others;
    others.reserve(state.size());
    SpatialGrid testGrid(largestBayDim(info_));

    for (int i = 0; i < (int)state.size(); ++i) {
        if (i == bayIndex) continue;
        testGrid.insertBay((int)others.size(), CollisionChecker::createSolidOBB(state[i], &info_));
        others.push_back(state[i]);
    }

    return CollisionChecker::isValidPlacement(candidate, others, &info_, &testGrid);
}

void JostleAlgorithm::greedyPlacement(std::vector<Bay>& state) {
    if (info_.bayTypes.empty()) return;

    double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
    for(auto p : info_.warehousePolygon) {
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
    }

    SpatialGrid testGrid(largestBayDim(info_));
    std::vector<BayType> sortedTypes = info_.bayTypes;
    // Heuristic: prioritize bays that add more area and loads
    std::sort(sortedTypes.begin(), sortedTypes.end(), [](const BayType& a, const BayType& b){
        return (a.width * a.depth * a.nLoads) > (b.width * b.depth * b.nLoads);
    });

    // Use a coarser grid for the initial greedy placement to speed it up
    double step = largestBayDim(info_) / 2.0;
    if (step < 50.0) step = 50.0;

    for (double y = minY; y <= maxY; y += step) {
        for (double x = minX; x <= maxX; x += step) {
            for (const auto& bt : sortedTypes) {
                for (double rot : {0.0, 90.0, 180.0, 270.0}) {
                    Bay b = {bt.id, x, y, rot};
                    if (CollisionChecker::isValidPlacement(b, state, &info_, &testGrid)) {
                        testGrid.insertBay((int)state.size(), CollisionChecker::createSolidOBB(b, &info_));
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

bool JostleAlgorithm::rotateBay(std::vector<Bay>& state, int bayIndex, double deltaAngle) {
    double oldRot = state[bayIndex].rotation;
    state[bayIndex].rotation += deltaAngle;
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
    if (info_.bayTypes.empty()) return false;
    std::uniform_int_distribution<int> type_dist(0, (int)info_.bayTypes.size() - 1);
    double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
    for(auto p : info_.warehousePolygon) {
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
    }
    std::uniform_real_distribution<double> x_dist(minX, maxX);
    std::uniform_real_distribution<double> y_dist(minY, maxY);
    std::uniform_real_distribution<double> rot_dist(0, 360);

    Bay newBay = {info_.bayTypes[type_dist(rng_)].id, x_dist(rng_), y_dist(rng_), rot_dist(rng_)};
    state.push_back(newBay);
    if (isBayValidInState(state, (int)state.size() - 1)) return true;
    state.pop_back();
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
    updateBest({current_state, current_score, name(), 0.0});

    std::uniform_real_distribution<double> move_dist(-50.0, 50.0);
    std::uniform_real_distribution<double> rot_dist(-45.0, 45.0);
    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
    std::discrete_distribution<int> op_dist({50, 5, 20, 20, 2, 3}); // Prioritize adding

    int iterations = 0;
    double temp = 1000.0;
    const double cooling = 0.9999;

    while (!stop_flag && (maxIterations_ <= 0 || iterations < maxIterations_)) {
        int op = op_dist(rng_);
        std::vector<Bay> next_state = current_state;
        bool success = false;

        if (op == 0) success = addRandomBay(next_state);
        else if (op == 1) success = removeRandomBay(next_state);
        else if (!next_state.empty()) {
            std::uniform_int_distribution<int> bay_dist(0, (int)next_state.size() - 1);
            int idx = bay_dist(rng_);
            if (op == 2) success = translateBay(next_state, idx, move_dist(rng_), move_dist(rng_));
            else if (op == 3) success = rotateBay(next_state, idx, rot_dist(rng_));
            else if (op == 4) success = swapBays(next_state, idx, (idx + 1) % (int)next_state.size());
            else if (op == 5) {
                std::uniform_int_distribution<int> type_dist(0, (int)info_.bayTypes.size() - 1);
                success = changeBayType(next_state, idx, info_.bayTypes[type_dist(rng_)].id);
            }
        }

        if (success) {
            double next_score = calculateScore(next_state);
            double delta = next_score - current_score;
            if (delta <= 0 || prob_dist(rng_) < std::exp(-delta / temp)) {
                current_state = std::move(next_state);
                current_score = next_score;
                if (current_score < best_.score) {
                    updateBest({current_state, current_score, name(), 0.0});
                }
            }
        }

        temp *= cooling;
        if (temp < 0.1) temp = 1000.0;
        iterations++;
    }
}
