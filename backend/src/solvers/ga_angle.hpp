#pragma once

#include "genetic_algorithm.hpp"

class GAAngle : public GeneticAlgorithm {
public:
    GAAngle(const StaticState& info, uint64_t seed);

    std::string name() const override { return "ga_angle"; }

    Solution decodeChromosome(const Chromosome& chromosome) override;
    Solution decodeContinuousBLF(const Chromosome& chromosome);
};
