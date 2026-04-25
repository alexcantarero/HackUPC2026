#include "genetic_algorithm.hpp"
#include "greedy.hpp"
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

// ---------------------------------------------------------------------------
// Post-decode greedy fill: after BLF places what it can, try to squeeze in
// more bays at any remaining anchor position. This is the main mechanism
// that lets GAs reach bay counts SA achieves — without it, the decoder gives
// up the moment a chromosome gene fails to place.
// ---------------------------------------------------------------------------
void GeneticAlgorithm::greedyFill(Solution& sol, std::atomic<bool>& stop_flag) const {
    if (info_.bayTypes.empty()) return;

    SpatialGrid fill_grid(defaultCellSize());
    for (int i = 0; i < static_cast<int>(sol.bays.size()); ++i)
        fill_grid.insertBay(i, CollisionChecker::createSolidOBB(sol.bays[i], &info_));

    // Build anchors from existing bay corners
    std::vector<Point2D> anchors = generateSafeAnchors();
    for (const auto& bay : sol.bays) {
        OBB solid = CollisionChecker::createSolidOBB(bay, &info_);
        OBB gap   = CollisionChecker::createGapOBB(bay, &info_);
        double min_x = solid.corners[0].x, max_x = min_x;
        double min_y = solid.corners[0].y, max_y = min_y;
        for (int i = 1; i < 4; ++i) {
            min_x = std::min(min_x, solid.corners[i].x); max_x = std::max(max_x, solid.corners[i].x);
            min_y = std::min(min_y, solid.corners[i].y); max_y = std::max(max_y, solid.corners[i].y);
        }
        for (int i = 0; i < 4; ++i) {
            min_x = std::min(min_x, gap.corners[i].x); max_x = std::max(max_x, gap.corners[i].x);
            min_y = std::min(min_y, gap.corners[i].y); max_y = std::max(max_y, gap.corners[i].y);
        }
        anchors.push_back({max_x, min_y});
        anchors.push_back({min_x, max_y});
        anchors.push_back(gap.center);
        double dx = gap.center.x - solid.center.x;
        double dy = gap.center.y - solid.center.y;
        anchors.push_back({gap.center.x + dx, gap.center.y + dy});
    }

    // Sort by bottom-left preference (standard BLF ordering)
    std::sort(anchors.begin(), anchors.end(), [](const Point2D& a, const Point2D& b) {
        auto q = [](double v) { return static_cast<long long>(std::round(v * 100.0)); };
        if (q(a.y) != q(b.y)) return q(a.y) < q(b.y);
        return q(a.x) < q(b.x);
    });

    // Sort bay types by efficiency (same as greedy)
    std::vector<int> type_order;
    for (int i = 0; i < static_cast<int>(info_.bayTypes.size()); ++i)
        type_order.push_back(i);
    std::sort(type_order.begin(), type_order.end(), [&](int a, int b) {
        const auto& ta = info_.bayTypes[a];
        const auto& tb = info_.bayTypes[b];
        double eff_a = (ta.width * ta.depth) / (ta.price / ta.nLoads);
        double eff_b = (tb.width * tb.depth) / (tb.price / tb.nLoads);
        return eff_a > eff_b;
    });

    bool placed_any = true;
    while (placed_any && !stop_flag.load()) {
        placed_any = false;
        for (int ti : type_order) {
            const BayType& bt = info_.bayTypes[ti];
            for (const auto& anchor : anchors) {
                for (double angle : {0.0, 90.0, 180.0, 270.0}) {
                    Bay candidate{bt.id, anchor.x, anchor.y, angle};
                    if (CollisionChecker::isValidPlacement(candidate, sol.bays, &info_, &fill_grid)) {
                        fill_grid.insertBay(static_cast<int>(sol.bays.size()),
                                            CollisionChecker::createSolidOBB(candidate, &info_));
                        sol.bays.push_back(candidate);
                        placed_any = true;
                        goto next_type; // one bay per type per pass, like greedy
                    }
                }
            }
            next_type:;
        }
    }
}

void GeneticAlgorithm::run(std::atomic<bool>& stop_flag) {
    const GAParams hp = params();
    if (info_.bayTypes.empty()) return;

    std::vector<Individual> population;
    population.reserve(static_cast<size_t>(hp.population_size));

    for (int i = 0; i < hp.population_size; ++i) {
        if (stop_flag.load()) break;

        Chromosome chromosome;
        if      (i == 0) chromosome = generateGreedyChromosome();
        else if (i == 1) chromosome = generateHeuristicChromosome(0);
        else if (i == 2) chromosome = generateHeuristicChromosome(1);
        else if (i == 3) chromosome = generateHeuristicChromosome(2);
        else             chromosome = randomChromosome();

        Solution solution = decodeChromosome(chromosome, stop_flag);
        greedyFill(solution, stop_flag);
        calculateMetrics(solution);

        const size_t padding = 20;
        if (solution.bays.size() > 0 && chromosome.size() > solution.bays.size() + padding)
            chromosome.erase(chromosome.begin() + solution.bays.size() + padding, chromosome.end());

        population.push_back({std::move(chromosome), std::move(solution)});
        updateBest(population.back().solution);
    }

    if (population.empty()) return;

    auto better_individual = [this](const Individual& lhs, const Individual& rhs) {
        return isBetterSolution(lhs.solution, rhs.solution);
    };

    auto tournament_select = [this, &population, &better_individual, &hp]() {
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

        const Solution previous_global_best = best_;
        updateBest(population.front().solution);

        if (best_.training_score < previous_global_best.training_score) stagnation = 0;
        else ++stagnation;

        // ── Diversity injection on deep stagnation ──────────────────────────
        // When stuck for 60+ generations, nuke the bottom half of the
        // population with fresh random individuals. Keeps elite survivors
        // (the good genes) while forcing the search into new regions.
        // This is the GA analog of SA's reheating seasons.
        if (stagnation > 0 && stagnation % 60 == 0) {
            int survivors = std::max(hp.elite_count, hp.population_size / 2);
            population.resize(survivors); // drop the weak half
            while (static_cast<int>(population.size()) < hp.population_size) {
                if (stop_flag.load()) break;
                Chromosome fresh = randomChromosome();
                // Occasionally seed from the global best with heavy perturbation
                // so we branch off the best known solution rather than random noise
                if (prob_dist(rng_) < 0.4 && !best_.bays.empty()) {
                    fresh = generateGreedyChromosome();
                    // Scramble a large random segment to break out of the basin
                    if (fresh.size() > 4) {
                        std::uniform_int_distribution<int> seg(0, static_cast<int>(fresh.size()) - 1);
                        int a = seg(rng_), b = seg(rng_);
                        if (a > b) std::swap(a, b);
                        std::shuffle(fresh.begin() + a, fresh.begin() + b + 1, rng_);
                    }
                }
                Solution sol = decodeChromosome(fresh, stop_flag);
                greedyFill(sol, stop_flag);
                calculateMetrics(sol);
                population.push_back({std::move(fresh), std::move(sol)});
                updateBest(population.back().solution);
            }
        }

        // ── Adaptive mutation rates ─────────────────────────────────────────
        // Rates scale from base→max as stagnation grows, resetting to base
        // after each diversity injection (stagnation resets on improvement).
        const double stagnation_factor = std::min(1.0, static_cast<double>(stagnation % 60) / 40.0);
        const double swap_rate     = hp.swap_rate_base     + (hp.swap_rate_max     - hp.swap_rate_base)     * stagnation_factor;
        const double scramble_rate = hp.scramble_rate_base + (hp.scramble_rate_max - hp.scramble_rate_base) * stagnation_factor;
        const double replace_rate  = hp.replace_rate_base  + (hp.replace_rate_max  - hp.replace_rate_base)  * stagnation_factor;
        const double spatial_rate  = hp.spatial_rate_base  + (hp.spatial_rate_max  - hp.spatial_rate_base)  * stagnation_factor;

        std::vector<Individual> next_population;
        next_population.reserve(static_cast<size_t>(hp.population_size));

        const int elite_count = std::min(hp.elite_count, static_cast<int>(population.size()));
        for (int i = 0; i < elite_count; ++i) next_population.push_back(population[i]);

        const int target_children = hp.population_size - hp.immigrants_per_generation;
        while (static_cast<int>(next_population.size()) < target_children) {
            if (stop_flag.load()) break;

            const int parent_a = tournament_select();
            const int parent_b = tournament_select();
            Chromosome child = crossoverTwoPoint(population[parent_a].chromosome,
                                                 population[parent_b].chromosome);

            if (prob_dist(rng_) < swap_rate)     mutateSwap(child);
            if (prob_dist(rng_) < scramble_rate) mutateScramble(child);
            if (prob_dist(rng_) < replace_rate)  mutateReplace(child);
            if (prob_dist(rng_) < spatial_rate)  mutateSpatial(child);

            Solution child_solution = decodeChromosome(child, stop_flag);
            greedyFill(child_solution, stop_flag);
            calculateMetrics(child_solution);

            const size_t padding = 20;
            if (child_solution.bays.size() > 0 && child.size() > child_solution.bays.size() + padding)
                child.erase(child.begin() + child_solution.bays.size() + padding, child.end());

            next_population.push_back({std::move(child), std::move(child_solution)});
        }

        while (static_cast<int>(next_population.size()) < hp.population_size) {
            if (stop_flag.load()) break;
            Chromosome immigrant = randomChromosome();
            Solution immigrant_solution = decodeChromosome(immigrant, stop_flag);
            greedyFill(immigrant_solution, stop_flag);
            calculateMetrics(immigrant_solution);

            const size_t padding = 20;
            if (immigrant_solution.bays.size() > 0 && immigrant.size() > immigrant_solution.bays.size() + padding)
                immigrant.erase(immigrant.begin() + immigrant_solution.bays.size() + padding, immigrant.end());

            next_population.push_back({std::move(immigrant), std::move(immigrant_solution)});
        }

        if (stop_flag.load()) break;

        for (const auto& individual : next_population) updateBest(individual.solution);
        population = std::move(next_population);
        generation++;
    }
}

GeneticAlgorithm::Chromosome GeneticAlgorithm::generateGreedyChromosome() {
    GreedySolver greedy(info_, rng_());
    greedy.fillPass();
    const Solution& greedy_sol = greedy.best();

    Chromosome chromosome;
    chromosome.reserve(greedy_sol.bays.size() + 50);
    for (const auto& bay : greedy_sol.bays)
        chromosome.push_back({bay.typeId, bay.x, bay.y, bay.rotation});

    Chromosome rand_chrom = randomChromosome();
    chromosome.insert(chromosome.end(), rand_chrom.begin(), rand_chrom.end());
    return chromosome;
}

GeneticAlgorithm::Chromosome GeneticAlgorithm::generateHeuristicChromosome(int type) {
    Chromosome chromosome = randomChromosome();

    if (type == 0) {
        std::sort(chromosome.begin(), chromosome.end(), [this](const SpatialGene& a, const SpatialGene& b) {
            const BayType* ba = getBayTypeById(a.bay_id);
            const BayType* bb = getBayTypeById(b.bay_id);
            if (!ba || !bb) return false;
            return (ba->width * ba->depth) > (bb->width * bb->depth);
        });
    } else if (type == 1) {
        std::sort(chromosome.begin(), chromosome.end(), [this](const SpatialGene& a, const SpatialGene& b) {
            const BayType* ba = getBayTypeById(a.bay_id);
            const BayType* bb = getBayTypeById(b.bay_id);
            if (!ba || !bb || ba->nLoads <= 0 || bb->nLoads <= 0) return false;
            return (ba->price / ba->nLoads) < (bb->price / bb->nLoads);
        });
    } else if (type == 2) {
        std::sort(chromosome.begin(), chromosome.end(), [this](const SpatialGene& a, const SpatialGene& b) {
            const BayType* ba = getBayTypeById(a.bay_id);
            const BayType* bb = getBayTypeById(b.bay_id);
            if (!ba || !bb || ba->nLoads <= 0 || bb->nLoads <= 0 || ba->price <= 0 || bb->price <= 0) return false;
            double eff_a = (ba->width * ba->depth) / (ba->price / ba->nLoads);
            double eff_b = (bb->width * bb->depth) / (bb->price / bb->nLoads);
            return eff_a > eff_b;
        });
    }
    return chromosome;
}

GeneticAlgorithm::Chromosome GeneticAlgorithm::randomChromosome() {
    Chromosome chromosome;
    double min_area = 1e9;
    std::vector<double> weights;

    for (const auto& bt : info_.bayTypes) {
        min_area = std::min(min_area, bt.width * bt.depth);
        if (bt.price > 0) weights.push_back(bt.nLoads / bt.price);
        else weights.push_back(0.1);
    }

    if (min_area <= 0.0) min_area = 1.0;
    int max_bays = static_cast<int>(std::ceil(warehouse_area_ / min_area));
    if (max_bays <= 0) max_bays = 1;
    if (max_bays > 5000) max_bays = 5000;

    std::discrete_distribution<int> bay_dist(weights.begin(), weights.end());

    double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
    for (auto p : info_.warehousePolygon) {
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
    }
    std::uniform_real_distribution<double> x_dist(minX, maxX);
    std::uniform_real_distribution<double> y_dist(minY, maxY);
    std::uniform_real_distribution<double> rot_dist(0.0, 360.0);

    chromosome.reserve(max_bays);
    for (int i = 0; i < max_bays; ++i)
        chromosome.push_back({info_.bayTypes[bay_dist(rng_)].id, x_dist(rng_), y_dist(rng_), rot_dist(rng_)});
    return chromosome;
}

GeneticAlgorithm::Chromosome GeneticAlgorithm::crossoverTwoPoint(const Chromosome& p1, const Chromosome& p2) {
    if (p1.empty()) return p2;
    if (p2.empty()) return p1;

    Chromosome child = (p1.size() > p2.size()) ? p1 : p2;
    const Chromosome& other = (p1.size() > p2.size()) ? p2 : p1;

    int min_len = static_cast<int>(other.size());
    std::uniform_int_distribution<int> dist(0, min_len - 1);

    int pt1 = dist(rng_), pt2 = dist(rng_);
    if (pt1 > pt2) std::swap(pt1, pt2);
    for (int i = pt1; i <= pt2; ++i) child[i] = other[i];
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
    std::uniform_int_distribution<int> bay_dist(0, static_cast<int>(info_.bayTypes.size()) - 1);
    chromosome[idx_dist(rng_)].bay_id = info_.bayTypes[bay_dist(rng_)].id;
}

void GeneticAlgorithm::mutateSpatial(Chromosome& chromosome) {
    if (chromosome.empty()) return;
    std::uniform_int_distribution<int> idx_dist(0, static_cast<int>(chromosome.size()) - 1);
    int idx = idx_dist(rng_);

    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);

    if (prob_dist(rng_) < 0.20) {
        double min_x = 1e9, max_x = -1e9, min_y = 1e9, max_y = -1e9;
        for (auto p : info_.warehousePolygon) {
            min_x = std::min(min_x, p.x); max_x = std::max(max_x, p.x);
            min_y = std::min(min_y, p.y); max_y = std::max(max_y, p.y);
        }
        std::uniform_real_distribution<double> x_dist(min_x, max_x);
        std::uniform_real_distribution<double> y_dist(min_y, max_y);
        chromosome[idx].target_x = x_dist(rng_);
        chromosome[idx].target_y = y_dist(rng_);
    } else {
        std::uniform_real_distribution<double> shift_dist(-1000.0, 1000.0);
        chromosome[idx].target_x += shift_dist(rng_);
        chromosome[idx].target_y += shift_dist(rng_);
    }

    std::uniform_real_distribution<double> rot_dist(-45.0, 45.0);
    chromosome[idx].target_rot = std::fmod(chromosome[idx].target_rot + rot_dist(rng_), 360.0);
    if (chromosome[idx].target_rot < 0.0) chromosome[idx].target_rot += 360.0;
}

const BayType* GeneticAlgorithm::getBayTypeById(int type_id) const {
    for (const auto& bay_type : info_.bayTypes)
        if (bay_type.id == type_id) return &bay_type;
    return nullptr;
}

double GeneticAlgorithm::randomAngleDegrees() {
    std::uniform_real_distribution<double> angle_dist(0.0, 360.0);
    return angle_dist(rng_);
}

bool GeneticAlgorithm::isBetterSolution(const Solution& lhs, const Solution& rhs) const {
    return lhs.training_score < rhs.training_score;
}

double GeneticAlgorithm::defaultCellSize() const {
    double cell_size = 1.0;
    for (const auto& bay_type : info_.bayTypes)
        cell_size = std::max(cell_size, std::max(bay_type.width, bay_type.depth));
    return cell_size;
}