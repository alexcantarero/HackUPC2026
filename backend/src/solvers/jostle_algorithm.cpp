#include "jostle_algorithm.hpp"

JostleAlgorithm::JostleAlgorithm(const StaticState& info, uint64_t seed, int maxIterations)
    : Algorithm(info, seed), maxIterations_(maxIterations) {
}

std::string JostleAlgorithm::name() const {
    return "Jostle Algorithm";
}

bool JostleAlgorithm::isValidState(const std::vector<Bay>& state) {
    // Check all bays against each other and static info.
    // To do this efficiently, we can rebuild the grid for the state.
    // But since isValidPlacement checks a candidate against ALREADY placed bays,
    // we can just check if all bays in 'state' can be placed sequentially.
    std::vector<Bay> placed;
    placed.reserve(state.size());
    SpatialGrid localGrid(grid_ /* initialize with same cell size? Wait, need to check if grid_ copy works */);
    
    // Instead, let's just do an O(N^2) check if the state is small, 
    // or use grid manually. We'll use a local grid.
    // But actually, we can just use the base class tryPlace, but that modifies best_.
    // Let's just create a grid.
    // We cannot access grid_ cell size easily, let's just use 1.0 or similar,
    // or we can use the base class info_.largestBaySize if available.
    SpatialGrid testGrid(5.0); // using 5.0 as fallback

    for (const Bay& bay : state) {
        if (!CollisionChecker::isValidPlacement(bay, placed, &info_, &testGrid)) {
            return false;
        }
        testGrid.insertBay(static_cast<int>(placed.size()), CollisionChecker::createSolidOBB(bay, &info_));
        placed.push_back(bay);
    }
    return true;
}

bool JostleAlgorithm::translateBay(std::vector<Bay>& state, int bayIndex, double deltaX, double deltaY) {
    if (bayIndex < 0 || bayIndex >= (int)state.size()) return false;
    
    std::vector<Bay> nextState = state;
    nextState[bayIndex].x += deltaX;
    nextState[bayIndex].y += deltaY;

    if (isValidState(nextState)) {
        state = std::move(nextState);
        return true;
    }
    return false;
}

bool JostleAlgorithm::rotateBay(std::vector<Bay>& state, int bayIndex, double deltaAngle) {
    if (bayIndex < 0 || bayIndex >= (int)state.size()) return false;
    
    std::vector<Bay> nextState = state;
    nextState[bayIndex].rotation += deltaAngle;

    // keep rotation in [0, 360)
    while (nextState[bayIndex].rotation >= 360.0) nextState[bayIndex].rotation -= 360.0;
    while (nextState[bayIndex].rotation < 0.0) nextState[bayIndex].rotation += 360.0;

    if (isValidState(nextState)) {
        state = std::move(nextState);
        return true;
    }
    return false;
}

bool JostleAlgorithm::swapBays(std::vector<Bay>& state, int bayIndex1, int bayIndex2) {
    if (bayIndex1 < 0 || bayIndex1 >= (int)state.size()) return false;
    if (bayIndex2 < 0 || bayIndex2 >= (int)state.size()) return false;
    if (bayIndex1 == bayIndex2) return true;
    
    std::vector<Bay> nextState = state;
    // Swap types
    std::swap(nextState[bayIndex1].typeId, nextState[bayIndex2].typeId);
    // Note: We might want to swap rotations too? Or just the type.
    // Swapping typeId means the new bays will be at the same locations.

    if (isValidState(nextState)) {
        state = std::move(nextState);
        return true;
    }
    return false;
}

bool JostleAlgorithm::changeBayType(std::vector<Bay>& state, int bayIndex, int newTypeId) {
    if (bayIndex < 0 || bayIndex >= (int)state.size()) return false;
    
    std::vector<Bay> nextState = state;
    nextState[bayIndex].typeId = newTypeId;

    if (isValidState(nextState)) {
        state = std::move(nextState);
        return true;
    }
    return false;
}

double JostleAlgorithm::polygonArea(const std::vector<Point2D>& polygon) {
    if (polygon.size() < 3) return 0.0;
    double twice_area = 0.0;
    for (size_t i = 0; i < polygon.size(); ++i) {
        const size_t j = (i + 1) % polygon.size();
        twice_area += polygon[i].x * polygon[j].y - polygon[j].x * polygon[i].y;
    }
    return std::abs(twice_area) * 0.5;
}

double JostleAlgorithm::evaluateQ(const std::vector<Bay>& state) const {
    double value_term = 0.0;
    double used_area = 0.0;

    for (const auto& bay : state) {
        const BayType* bay_type = CollisionChecker::getBayType(bay.typeId, &info_);
        if (bay_type == nullptr || bay_type->nLoads <= 0.0) continue;
        value_term += bay_type->price / bay_type->nLoads;
        used_area += bay_type->width * bay_type->depth;
    }

    const double warehouse_area = polygonArea(info_.warehousePolygon);
    if (warehouse_area <= 0.0) return std::numeric_limits<double>::max();

    return value_term * value_term - (used_area / warehouse_area);
}

void JostleAlgorithm::run(std::atomic<bool>& stop_flag) {
    // Basic initialization: start with an empty layout or generate a random initial state
    // For a local search, it's better to start with some valid state. We'll start empty.
    std::vector<Bay> current_state;
    double current_score = evaluateQ(current_state);
    
    Solution initialSol;
    initialSol.bays = current_state;
    initialSol.score = current_score;
    updateBest(initialSol);

    std::uniform_real_distribution<double> move_dist(-1.0, 1.0);
    std::uniform_real_distribution<double> rot_dist(-15.0, 15.0);
    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
    std::uniform_int_distribution<int> op_dist(0, 3);

    int iterations = 0;
    int stagnation = 0;
    double temp = 100.0;
    const double cooling_rate = 0.99;

    while (!stop_flag && iterations < maxIterations_) {
        // If empty, try to insert a random bay
        if (current_state.empty()) {
            if (info_.bayTypes.empty()) break;
            std::uniform_int_distribution<int> type_dist(0, info_.bayTypes.size() - 1);
            Bay newBay;
            newBay.typeId = info_.bayTypes[type_dist(rng_)].id;
            
            // Random point in warehouse bounding box roughly
            double minX = info_.warehousePolygon[0].x, maxX = minX;
            double minY = info_.warehousePolygon[0].y, maxY = minY;
            for(auto p : info_.warehousePolygon) {
                if(p.x < minX) minX = p.x;
                if(p.x > maxX) maxX = p.x;
                if(p.y < minY) minY = p.y;
                if(p.y > maxY) maxY = p.y;
            }
            std::uniform_real_distribution<double> x_dist(minX, maxX);
            std::uniform_real_distribution<double> y_dist(minY, maxY);
            std::uniform_real_distribution<double> init_rot_dist(0.0, 360.0);

            newBay.x = x_dist(rng_);
            newBay.y = y_dist(rng_);
            newBay.rotation = init_rot_dist(rng_);

            current_state.push_back(newBay);
            if (!isValidState(current_state)) {
                current_state.pop_back(); // revert
            }
        } else {
            // Apply a random operator to a random bay
            std::uniform_int_distribution<int> bay_dist(0, current_state.size() - 1);
            int bayIndex = bay_dist(rng_);
            int op = op_dist(rng_);
            
            std::vector<Bay> neighbor = current_state;
            bool moved = false;

            if (op == 0) { // Translate
                moved = translateBay(neighbor, bayIndex, move_dist(rng_), move_dist(rng_));
            } else if (op == 1) { // Rotate
                moved = rotateBay(neighbor, bayIndex, rot_dist(rng_));
            } else if (op == 2) { // Swap
                if (current_state.size() > 1) {
                    int bayIndex2 = bay_dist(rng_);
                    moved = swapBays(neighbor, bayIndex, bayIndex2);
                }
            } else if (op == 3) { // Change Type
                if (!info_.bayTypes.empty()) {
                    std::uniform_int_distribution<int> type_dist(0, info_.bayTypes.size() - 1);
                    moved = changeBayType(neighbor, bayIndex, info_.bayTypes[type_dist(rng_)].id);
                }
            }

            if (moved) {
                double neighbor_score = evaluateQ(neighbor);
                double delta = neighbor_score - current_score;

                // Simulated Annealing acceptance
                if (delta < 0 || prob_dist(rng_) < std::exp(-delta / temp)) {
                    current_state = std::move(neighbor);
                    current_score = neighbor_score;

                    if (current_score < best_.score) {
                        Solution sol;
                        sol.bays = current_state;
                        sol.score = current_score;
                        updateBest(sol);
                        stagnation = 0;
                    } else {
                        stagnation++;
                    }
                } else {
                    stagnation++;
                }
            } else {
                stagnation++;
            }
        }

        temp *= cooling_rate;
        if (temp < 1e-3) temp = 100.0; // reheat
        iterations++;
    }
}
