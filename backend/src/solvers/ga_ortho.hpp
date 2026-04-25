#pragma once

#include "algorithm.hpp"
#include <vector>

class GAOrtho : public Algorithm {
public:
    using Chromosome = std::vector<int>;

    GAOrtho(const StaticState& info, uint64_t seed);
    
    void run(std::atomic<bool>& stop_flag) override;
    std::string name() const override { return "ga_ortho"; }

    // Phase 1: Genetic Sequence Operators
    Chromosome crossoverOX1(const Chromosome& p1, const Chromosome& p2);
    void mutateSwap(Chromosome& chromosome);
    void mutateScramble(Chromosome& chromosome);
};
