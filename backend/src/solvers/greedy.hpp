#pragma once
#include "algorithm.hpp"
#include <array>
#include <vector>

class GreedySolver : public Algorithm {
public:
    GreedySolver(const StaticState& info, uint64_t seed);

    void        run(std::atomic<bool>& stop_flag) override;
    std::string name() const override { return "greedy"; }
    bool        isAnytime() const override { return false; } // Greedy runs once and finishes

    void fillPass();

private:
    std::vector<int> sorted_type_ids_; 
    static constexpr size_t NUM_ROTATIONS = 13;
    std::array<double, NUM_ROTATIONS> rotations_; 
};