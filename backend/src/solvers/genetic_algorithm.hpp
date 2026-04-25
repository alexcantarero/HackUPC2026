#pragma once

#include "algorithm.hpp"
#include <vector>

class GeneticAlgorithm : public Algorithm {
public:
    using Chromosome = std::vector<int>;

    GeneticAlgorithm(const StaticState& info, uint64_t seed);

    void run(std::atomic<bool>& stop_flag) override;

    Chromosome crossoverOX1(const Chromosome& p1, const Chromosome& p2);
    void mutateSwap(Chromosome& chromosome);
    void mutateScramble(Chromosome& chromosome);

protected:
    struct GAParams {
        int population_size = 24;
        int elite_count = 4;
        int tournament_size = 3;
        int immigrants_per_generation = 2;
        double swap_rate_base = 0.25;
        double swap_rate_max = 0.55;
        double scramble_rate_base = 0.10;
        double scramble_rate_max = 0.35;
    };

    // Passed stop_flag so it can exit mid-decode if time is up!
    virtual Solution decodeChromosome(const Chromosome& chromosome, std::atomic<bool>& stop_flag) = 0;

    Chromosome randomChromosome();
    const BayType* getBayTypeById(int type_id) const;
    double randomAngleDegrees();
    bool isBetterSolution(const Solution& lhs, const Solution& rhs) const;
    double solutionFootprintArea(const Solution& solution) const;
    GAParams params() const;
    double defaultCellSize() const;
};