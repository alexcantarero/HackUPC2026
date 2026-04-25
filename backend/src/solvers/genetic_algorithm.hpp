#pragma once

#include "algorithm.hpp"
#include <vector>

// The GA evolves Target Locations, not just sequences.
struct SpatialGene {
    int bay_id;
    double target_x;
    double target_y;
    double target_rot;
};

class GeneticAlgorithm : public Algorithm {
public:
    using Chromosome = std::vector<SpatialGene>;

    struct GAParams {
        int population_size = 24;
        int elite_count = 4;
        int tournament_size = 3;
        int immigrants_per_generation = 2;
        double swap_rate_base = 0.25, swap_rate_max = 0.55;
        double scramble_rate_base = 0.10, scramble_rate_max = 0.35;
        double replace_rate_base = 0.15, replace_rate_max = 0.40;
        double spatial_rate_base = 0.30, spatial_rate_max = 0.70;
    };

    GeneticAlgorithm(const StaticState& info, uint64_t seed);
    void setParams(const GAParams& p) { hp_ = p; }

    void run(std::atomic<bool>& stop_flag) override;

    Chromosome crossoverTwoPoint(const Chromosome& p1, const Chromosome& p2);
    void mutateSwap(Chromosome& chromosome);
    void mutateScramble(Chromosome& chromosome);
    void mutateReplace(Chromosome& chromosome);
    void mutateSpatial(Chromosome& chromosome);

protected:
    GAParams hp_;
    GAParams params() const { return hp_; }

    virtual Solution decodeChromosome(const Chromosome& chromosome, std::atomic<bool>& stop_flag) = 0;

    // After BLF decode, greedily fill any remaining space.
    // This is what allows GAs to reach the same bay count as SA.
    void greedyFill(Solution& sol, std::atomic<bool>& stop_flag) const;

    Chromosome randomChromosome();
    Chromosome generateHeuristicChromosome(int type);
    Chromosome generateGreedyChromosome();

    const BayType* getBayTypeById(int type_id) const;
    double randomAngleDegrees();
    bool isBetterSolution(const Solution& lhs, const Solution& rhs) const;
    double defaultCellSize() const;
};