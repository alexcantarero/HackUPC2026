#include "jostle_algorithm.hpp"

JostleAlgorithm::JostleAlgorithm(const StaticState& info, uint64_t seed, int maxIterations)
    : Algorithm(info, seed), maxIterations_(maxIterations) {
}

std::string JostleAlgorithm::name() const {
    return "Jostle Algorithm";
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
