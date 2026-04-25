#include "genetic_algorithm.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <unordered_set>
#include "../core/score.hpp" // Add this to the top of genetic_algorithm.cpp

namespace {

struct Individual {
    GeneticAlgorithm::Chromosome chromosome;
    Solution solution;
};

}  // namespace

GeneticAlgorithm::GeneticAlgorithm(const StaticState& info, uint64_t seed)
    : Algorithm(info, seed) {
}

void GeneticAlgorithm::run(std::atomic<bool>& stop_flag) {
    const GAParams hp = params();
    if (info_.bayTypes.empty()) {
        return;
    }

    std::vector<Individual> population;
    population.reserve(static_cast<size_t>(hp.population_size));
    for (int i = 0; i < hp.population_size; ++i) {
        Chromosome chromosome = randomChromosome();
        Solution solution = decodeChromosome(chromosome);
        population.push_back({std::move(chromosome), std::move(solution)});
        updateBest(population.back().solution);
    }

    auto better_individual = [this](const Individual& lhs, const Individual& rhs) {
        return isBetterSolution(lhs.solution, rhs.solution);
    };

    auto tournament_select = [this, &population, &better_individual, &hp]() {
        std::uniform_int_distribution<int> pick_dist(
            0,
            static_cast<int>(population.size()) - 1);
        int best_idx = pick_dist(rng_);
        for (int i = 1; i < hp.tournament_size; ++i) {
            int idx = pick_dist(rng_);
            if (better_individual(population[idx], population[best_idx])) {
                best_idx = idx;
            }
        }
        return best_idx;
    };

    int stagnation = 0;
    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);

    while (!stop_flag) {
        std::sort(population.begin(), population.end(), better_individual);

        const Solution generation_best = population.front().solution;
        const Solution previous_global_best = best_;
        updateBest(generation_best);
        if (best_.score < previous_global_best.score) {
            stagnation = 0;
        } else {
            ++stagnation;
        }

        const double stagnation_factor =
            std::min(1.0, static_cast<double>(stagnation) / 40.0);
        const double swap_rate = hp.swap_rate_base +
            (hp.swap_rate_max - hp.swap_rate_base) * stagnation_factor;
        const double scramble_rate = hp.scramble_rate_base +
            (hp.scramble_rate_max - hp.scramble_rate_base) * stagnation_factor;

        std::vector<Individual> next_population;
        next_population.reserve(static_cast<size_t>(hp.population_size));

        const int elite_count = std::min(
            hp.elite_count,
            static_cast<int>(population.size()));
        for (int i = 0; i < elite_count; ++i) {
            next_population.push_back(population[i]);
        }

        const int target_children = hp.population_size - hp.immigrants_per_generation;
        while (static_cast<int>(next_population.size()) < target_children) {
            const int parent_a = tournament_select();
            const int parent_b = tournament_select();

            Chromosome child = crossoverOX1(
                population[parent_a].chromosome,
                population[parent_b].chromosome);

            if (prob_dist(rng_) < swap_rate) {
                mutateSwap(child);
            }
            if (prob_dist(rng_) < scramble_rate) {
                mutateScramble(child);
            }

            Solution child_solution = decodeChromosome(child);
            next_population.push_back({std::move(child), std::move(child_solution)});
        }

        while (static_cast<int>(next_population.size()) < hp.population_size) {
            Chromosome immigrant = randomChromosome();
            Solution immigrant_solution = decodeChromosome(immigrant);
            next_population.push_back(
                {std::move(immigrant), std::move(immigrant_solution)});
        }

        for (const auto& individual : next_population) {
            updateBest(individual.solution);
        }

        population = std::move(next_population);
    }
}

GeneticAlgorithm::Chromosome GeneticAlgorithm::randomChromosome() {
    Chromosome chromosome;
    
    // 1. Find the smallest bay area
    double min_area = std::numeric_limits<double>::max();
    for (const auto& bt : info_.bayTypes) {
        min_area = std::min(min_area, bt.width * bt.depth);
    }
    
    // 2. Calculate dynamic upper bound
    double wh_area = polygonArea(info_.warehousePolygon);
    int max_bays = static_cast<int>(std::ceil(wh_area / min_area));
    if (max_bays <= 0) max_bays = 1; // Safety fallback

    // 3. Populate chromosome
    chromosome.reserve(max_bays);
    std::uniform_int_distribution<int> dist(0, static_cast<int>(info_.bayTypes.size()) - 1);
    for (int i = 0; i < max_bays; ++i) {
        chromosome.push_back(info_.bayTypes[dist(rng_)].id);
    }
    
    return chromosome;
}

GeneticAlgorithm::Chromosome GeneticAlgorithm::crossoverOX1(
    const Chromosome& p1,
    const Chromosome& p2) {
    if (p1.empty() || p1.size() != p2.size()) {
        return p1;
    }

    const int n = static_cast<int>(p1.size());
    Chromosome child(n, -1);

    std::uniform_int_distribution<int> dist(0, n - 1);
    int start = dist(rng_);
    int end = dist(rng_);
    if (start > end) {
        std::swap(start, end);
    }

    std::unordered_set<int> copied;
    for (int i = start; i <= end; ++i) {
        child[i] = p1[i];
        copied.insert(p1[i]);
    }

    int child_idx = (end + 1) % n;
    int p2_idx = (end + 1) % n;
    for (int i = 0; i < n; ++i) {
        const int candidate = p2[p2_idx];
        if (copied.find(candidate) == copied.end()) {
            child[child_idx] = candidate;
            child_idx = (child_idx + 1) % n;
        }
        p2_idx = (p2_idx + 1) % n;
    }

    return child;
}

void GeneticAlgorithm::mutateSwap(Chromosome& chromosome) {
    if (chromosome.size() < 2) {
        return;
    }

    std::uniform_int_distribution<int> dist(
        0,
        static_cast<int>(chromosome.size()) - 1);
    const int idx1 = dist(rng_);
    const int idx2 = dist(rng_);
    std::swap(chromosome[idx1], chromosome[idx2]);
}

void GeneticAlgorithm::mutateScramble(Chromosome& chromosome) {
    if (chromosome.size() < 2) {
        return;
    }

    std::uniform_int_distribution<int> dist(
        0,
        static_cast<int>(chromosome.size()) - 1);
    int start = dist(rng_);
    int end = dist(rng_);
    if (start > end) {
        std::swap(start, end);
    }
    std::shuffle(chromosome.begin() + start, chromosome.begin() + end + 1, rng_);
}

const BayType* GeneticAlgorithm::getBayTypeById(int type_id) const {
    for (const auto& bay_type : info_.bayTypes) {
        if (bay_type.id == type_id) {
            return &bay_type;
        }
    }
    return nullptr;
}

double GeneticAlgorithm::evaluateQ(const Solution& solution) const {
    double wh_area = polygonArea(info_.warehousePolygon);
    return computeScore(solution.bays, info_, wh_area);
}

double GeneticAlgorithm::polygonArea(const std::vector<Point2D>& polygon) {
    if (polygon.size() < 3) {
        return 0.0;
    }

    double twice_area = 0.0;
    for (size_t i = 0; i < polygon.size(); ++i) {
        const size_t j = (i + 1) % polygon.size();
        twice_area += polygon[i].x * polygon[j].y - polygon[j].x * polygon[i].y;
    }
    return std::abs(twice_area) * 0.5;
}

double GeneticAlgorithm::randomAngleDegrees() {
    std::uniform_real_distribution<double> angle_dist(0.0, 360.0);
    return angle_dist(rng_);
}

bool GeneticAlgorithm::isBetterSolution(
    const Solution& lhs,
    const Solution& rhs) const {
    constexpr double kScoreEps = 1e-9;
    if (lhs.score + kScoreEps < rhs.score) {
        return true;
    }
    if (rhs.score + kScoreEps < lhs.score) {
        return false;
    }
    if (lhs.bays.size() != rhs.bays.size()) {
        return lhs.bays.size() > rhs.bays.size();
    }
    return solutionFootprintArea(lhs) < solutionFootprintArea(rhs);
}

double GeneticAlgorithm::solutionFootprintArea(const Solution& solution) const {
    if (solution.bays.empty()) {
        return 0.0;
    }

    double min_x = std::numeric_limits<double>::max();
    double max_x = -std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();
    double max_y = -std::numeric_limits<double>::max();

    for (const auto& bay : solution.bays) {
        OBB solid = CollisionChecker::createSolidOBB(bay, &info_);
        for (const auto& corner : solid.corners) {
            min_x = std::min(min_x, corner.x);
            max_x = std::max(max_x, corner.x);
            min_y = std::min(min_y, corner.y);
            max_y = std::max(max_y, corner.y);
        }
    }

    return (max_x - min_x) * (max_y - min_y);
}

GeneticAlgorithm::GAParams GeneticAlgorithm::params() const {
    return {};
}

double GeneticAlgorithm::defaultCellSize() const {
    double cell_size = 1.0;
    for (const auto& bay_type : info_.bayTypes) {
        cell_size = std::max(cell_size, std::max(bay_type.width, bay_type.depth));
    }
    return cell_size;
}
