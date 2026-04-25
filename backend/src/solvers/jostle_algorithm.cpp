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

void JostleAlgorithm::run(std::atomic<bool>& stop_flag) {
    // Initial dummy state or starting state logic will go here.
    // For now, we just loop until stop_flag or maxIterations is reached.
    int iterations = 0;
    while (!stop_flag && iterations < maxIterations_) {
        // TODO: Implement local search logic here
        
        iterations++;
    }
}
