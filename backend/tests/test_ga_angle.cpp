#include "../src/solvers/ga_angle.hpp"
#include "../src/core/types.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

namespace {

StaticState makeStateForDecode() {
    StaticState state;
    state.warehousePolygon = {
        {0, 0}, {1000, 0}, {1000, 1000}, {0, 1000}
    };
    state.ceilingRegions = {{0, 1000}};
    state.bayTypes.push_back({1, 100, 100, 10, 20, 2, 10});
    state.bayTypes.push_back({2, 120, 80, 10, 20, 5, 5});
    return state;
}

double expectedQForAllPlaced() {
    const double sum_ratio = (10.0 / 2.0) + (5.0 / 5.0);
    const double used_area = (100.0 * 100.0) + (120.0 * 80.0);
    const double warehouse_area = 1000.0 * 1000.0;
    return sum_ratio * sum_ratio - (used_area / warehouse_area);
}

}  // namespace

void test_genetic_operators() {
    StaticState state = makeStateForDecode();
    GAAngle ga(state, 42);

    GeneticAlgorithm::Chromosome p1 = {1, 2};
    GeneticAlgorithm::Chromosome p2 = {2, 1};
    GeneticAlgorithm::Chromosome child = ga.crossoverOX1(p1, p2);

    assert(child.size() == p1.size());
    std::sort(child.begin(), child.end());
    assert(child[0] == 1);
    assert(child[1] == 2);

    GeneticAlgorithm::Chromosome c = {1, 2};
    ga.mutateSwap(c);
    std::sort(c.begin(), c.end());
    assert(c[0] == 1);
    assert(c[1] == 2);

    std::cout << "test_genetic_operators passed!" << std::endl;
}

void test_decode_and_fitness() {
    StaticState state = makeStateForDecode();
    GAAngle ga(state, 7);

    GeneticAlgorithm::Chromosome chromosome = {1, 2};
    Solution decoded = ga.decodeContinuousBLF(chromosome);

    assert(decoded.bays.size() == 2);
    assert(std::abs(decoded.bays[0].x) < 1e-9);
    assert(std::abs(decoded.bays[0].y) < 1e-9);
    for (const auto& bay : decoded.bays) {
        assert(bay.rotation >= 0.0);
        assert(bay.rotation <= 360.0);
    }

    const double expected = expectedQForAllPlaced();
    const double actual = ga.evaluateQ(decoded);
    assert(std::abs(actual - expected) < 1e-9);
    std::cout << "test_decode_and_fitness passed!" << std::endl;
}

int main() {
    test_genetic_operators();
    test_decode_and_fitness();
    std::cout << "All GAAngle tests passed!" << std::endl;
    return 0;
}
