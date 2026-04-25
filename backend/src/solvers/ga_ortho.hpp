#pragma once

#include "genetic_algorithm.hpp"

class GAOrtho : public GeneticAlgorithm {
public:
    GAOrtho(const StaticState& info, uint64_t seed);

    std::string name() const override { return "ga_ortho"; }

    // Phase 2: Orthogonal BLF decoder and fitness
    Solution decodeChromosome(const Chromosome& chromosome) override;
    Solution decodeOrthogonalBLF(const Chromosome& chromosome);
};
