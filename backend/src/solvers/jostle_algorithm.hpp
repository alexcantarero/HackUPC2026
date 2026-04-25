#pragma once

#include "algorithm.hpp"

class JostleAlgorithm : public Algorithm {
public:
    JostleAlgorithm(const StaticState& info, uint64_t seed, int maxIterations);

    void run(std::atomic<bool>& stop_flag) override;
    std::string name() const override;

private:
    int maxIterations_;
};
