#pragma once
#include "genetic_algorithm.hpp"

class GAOrtho : public GeneticAlgorithm {
public:
    GAOrtho(const StaticState& info, uint64_t seed);
    std::string name() const override { return "ga_ortho"; }
protected:
    Solution decodeChromosome(const Chromosome& chromosome, std::atomic<bool>& stop_flag) override;
    Solution decodeOrthogonalBLF(const Chromosome& chromosome, std::atomic<bool>& stop_flag);
};