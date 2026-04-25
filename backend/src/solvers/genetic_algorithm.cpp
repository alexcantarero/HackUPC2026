#include "genetic_algorithm.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <iostream>

namespace {
struct Individual {
    GeneticAlgorithm::Chromosome chromosome;
    Solution solution;
};
}  

GeneticAlgorithm::GeneticAlgorithm(const StaticState& info, uint64_t seed)
    : Algorithm(info, seed) {}

void GeneticAlgorithm::run(std::atomic<bool>& stop_flag) {
    const GAParams hp = params();
    if (info_.bayTypes.empty()) return;

    std::vector<Individual> population;
    population.reserve(static_cast<size_t>(hp.population_size));
    
    std::cout << "[DEBUG] " << name() << " starting Generation 0...\n";
    
    for (int i = 0; i < hp.population_size; ++i) {
        if (stop_flag.load()) break; 
        Chromosome chromosome = randomChromosome();
        Solution solution = decodeChromosome(chromosome, stop_flag);
        population.push_back({std::move(chromosome), std::move(solution)});
        updateBest(population.back().solution);
    }

    auto better_individual = [this](const Individual& lhs, const Individual& rhs) {
        return isBetterSolution(lhs.solution, rhs.solution);
    };

    auto tournament_select =[this, &population, &better_individual, &hp]() {
        std::uniform_int_distribution<int> pick_dist(0, static_cast<int>(population.size()) - 1);
        int best_idx = pick_dist(rng_);
        for (int i = 1; i < hp.tournament_size; ++i) {
            int idx = pick_dist(rng_);
            if (better_individual(population[idx], population[best_idx])) best_idx = idx;
        }
        return best_idx;
    };

    int stagnation = 0;
    int generation = 1;
    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);

    while (!stop_flag.load()) {
        std::sort(population.begin(), population.end(), better_individual);

        const Solution generation_best = population.front().solution;
        const Solution previous_global_best = best_;
        updateBest(generation_best);
        
        if (best_.training_score < previous_global_best.training_score) stagnation = 0;
        else ++stagnation;

        const double stagnation_factor = std::min(1.0, static_cast<double>(stagnation) / 40.0);
        const double swap_rate = hp.swap_rate_base + (hp.swap_rate_max - hp.swap_rate_base) * stagnation_factor;
        const double scramble_rate = hp.scramble_rate_base + (hp.scramble_rate_max - hp.scramble_rate_base) * stagnation_factor;
        const double replace_rate = hp.replace_rate_base + (hp.replace_rate_max - hp.replace_rate_base) * stagnation_factor;

        std::vector<Individual> next_population;
        next_population.reserve(static_cast<size_t>(hp.population_size));

        const int elite_count = std::min(hp.elite_count, static_cast<int>(population.size()));
        for (int i = 0; i < elite_count; ++i) next_population.push_back(population[i]);

        const int target_children = hp.population_size - hp.immigrants_per_generation;
        while (static_cast<int>(next_population.size()) < target_children) {
            if (stop_flag.load()) break; 
            
            const int parent_a = tournament_select();
            const int parent_b = tournament_select();

            // FAST TWO-POINT CROSSOVER
            Chromosome child = crossoverTwoPoint(population[parent_a].chromosome, population[parent_b].chromosome);

            if (prob_dist(rng_) < swap_rate) mutateSwap(child);
            if (prob_dist(rng_) < scramble_rate) mutateScramble(child);
            if (prob_dist(rng_) < replace_rate) mutateReplace(child);

            Solution child_solution = decodeChromosome(child, stop_flag);
            next_population.push_back({std::move(child), std::move(child_solution)});
        }

        while (static_cast<int>(next_population.size()) < hp.population_size) {
            if (stop_flag.load()) break; 
            Chromosome immigrant = randomChromosome();
            Solution immigrant_solution = decodeChromosome(immigrant, stop_flag);
            next_population.push_back({std::move(immigrant), std::move(immigrant_solution)});
        }

        for (const auto& individual : next_population) updateBest(individual.solution);
        population = std::move(next_population);
        generation++;
    }
}

// BIASED INITIALIZATION: Picks bays that maximize Loads/Price
GeneticAlgorithm::Chromosome GeneticAlgorithm::randomChromosome() {
    Chromosome chromosome;
    
    double min_area = std::numeric_limits<double>::max();
    std::vector<double> weights;
    for (const auto& bt : info_.bayTypes) {
        min_area = std::min(min_area, bt.width * bt.depth);
        weights.push_back(bt.nLoads / bt.price); // High weight = highly profitable bay
    }
    
    int max_bays = static_cast<int>(std::ceil(warehouse_area_ / min_area));
    if (max_bays <= 0) max_bays = 1;

    std::discrete_distribution<int> bay_dist(weights.begin(), weights.end());
    chromosome.reserve(max_bays);
    for (int i = 0; i < max_bays; ++i) {
        chromosome.push_back(info_.bayTypes[bay_dist(rng_)].id);
    }
    
    return chromosome;
}

// TWO-POINT CROSSOVER: Lightning fast, handles duplicates perfectly.
GeneticAlgorithm::Chromosome GeneticAlgorithm::crossoverTwoPoint(const Chromosome& p1, const Chromosome& p2) {
    if (p1.empty() || p1.size() != p2.size()) return p1;
    
    int n = static_cast<int>(p1.size());
    Chromosome child = p1;
    
    std::uniform_int_distribution<int> dist(0, n - 1);
    int pt1 = dist(rng_), pt2 = dist(rng_);
    if (pt1 > pt2) std::swap(pt1, pt2);
    
    for (int i = pt1; i <= pt2; ++i) {
        child[i] = p2[i];
    }
    return child;
}

void GeneticAlgorithm::mutateSwap(Chromosome& chromosome) {
    if (chromosome.size() < 2) return;
    std::uniform_int_distribution<int> dist(0, static_cast<int>(chromosome.size()) - 1);
    std::swap(chromosome[dist(rng_)], chromosome[dist(rng_)]);
}

void GeneticAlgorithm::mutateScramble(Chromosome& chromosome) {
    if (chromosome.size() < 2) return;
    std::uniform_int_distribution<int> dist(0, static_cast<int>(chromosome.size()) - 1);
    int start = dist(rng_), end = dist(rng_);
    if (start > end) std::swap(start, end);
    std::shuffle(chromosome.begin() + start, chromosome.begin() + end + 1, rng_);
}

void GeneticAlgorithm::mutateReplace(Chromosome& chromosome) {
    if (chromosome.empty()) return;
    std::uniform_int_distribution<int> idx_dist(0, static_cast<int>(chromosome.size()) - 1);
    
    std::vector<double> weights;
    for (const auto& bt : info_.bayTypes) weights.push_back(bt.nLoads / bt.price);
    std::discrete_distribution<int> bay_dist(weights.begin(), weights.end());
    
    chromosome[idx_dist(rng_)] = info_.bayTypes[bay_dist(rng_)].id;
}

const BayType* GeneticAlgorithm::getBayTypeById(int type_id) const {
    for (const auto& bay_type : info_.bayTypes) {
        if (bay_type.id == type_id) return &bay_type;
    }
    return nullptr;
}

double GeneticAlgorithm::randomAngleDegrees() {
    std::uniform_real_distribution<double> angle_dist(0.0, 360.0);
    return angle_dist(rng_);
}

bool GeneticAlgorithm::isBetterSolution(const Solution& lhs, const Solution& rhs) const {
    return lhs.training_score < rhs.training_score;
}

double GeneticAlgorithm::solutionFootprintArea(const Solution& solution) const {
    if (solution.bays.empty()) return 0.0;
    double min_x = std::numeric_limits<double>::max(), max_x = -std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max(), max_y = -std::numeric_limits<double>::max();

    for (const auto& bay : solution.bays) {
        OBB solid = CollisionChecker::createSolidOBB(bay, &info_);
        for (const auto& corner : solid.corners) {
            min_x = std::min(min_x, corner.x); max_x = std::max(max_x, corner.x);
            min_y = std::min(min_y, corner.y); max_y = std::max(max_y, corner.y);
        }
    }
    return (max_x - min_x) * (max_y - min_y);
}

GeneticAlgorithm::GAParams GeneticAlgorithm::params() const { return {}; }
double GeneticAlgorithm::defaultCellSize() const {
    double cell_size = 1.0;
    for (const auto& bay_type : info_.bayTypes) cell_size = std::max(cell_size, std::max(bay_type.width, bay_type.depth));
    return cell_size;
}