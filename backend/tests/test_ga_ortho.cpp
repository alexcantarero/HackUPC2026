#include "../src/solvers/ga_ortho.hpp"
#include "../src/core/types.hpp"
#include <iostream>
#include <cassert>
#include <algorithm>
#include <cmath>

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

void test_crossover() {
    StaticState state;
    GAOrtho ga(state, 42); // fixed seed
    
    GAOrtho::Chromosome p1 = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    GAOrtho::Chromosome p2 = {9, 3, 7, 8, 2, 6, 5, 1, 4};
    
    GAOrtho::Chromosome child = ga.crossoverOX1(p1, p2);
    
    assert(child.size() == p1.size());
    // Check that all elements are unique (valid permutation)
    GAOrtho::Chromosome sorted_child = child;
    std::sort(sorted_child.begin(), sorted_child.end());
    for (size_t i = 0; i < sorted_child.size(); ++i) {
        assert(sorted_child[i] == static_cast<int>(i + 1));
    }
    std::cout << "test_crossover passed!" << std::endl;
}

void test_mutation() {
    StaticState state;
    GAOrtho ga(state, 42); // fixed seed
    
    GAOrtho::Chromosome c = {1, 2, 3, 4, 5};
    ga.mutateSwap(c);
    
    assert(c.size() == 5);
    GAOrtho::Chromosome sorted_c = c;
    std::sort(sorted_c.begin(), sorted_c.end());
    for (size_t i = 0; i < sorted_c.size(); ++i) {
        assert(sorted_c[i] == static_cast<int>(i + 1));
    }
    std::cout << "test_mutation swap passed!" << std::endl;

    GAOrtho::Chromosome c2 = {1, 2, 3, 4, 5};
    ga.mutateScramble(c2);
    assert(c2.size() == 5);
    GAOrtho::Chromosome sorted_c2 = c2;
    std::sort(sorted_c2.begin(), sorted_c2.end());
    for (size_t i = 0; i < sorted_c2.size(); ++i) {
        assert(sorted_c2[i] == static_cast<int>(i + 1));
    }
    std::cout << "test_mutation scramble passed!" << std::endl;
}

void test_decode_and_fitness() {
    StaticState state = makeStateForDecode();
    GAOrtho ga(state, 7);

    GAOrtho::Chromosome chromosome = {1, 2};
    Solution decoded = ga.decodeOrthogonalBLF(chromosome);

    assert(decoded.bays.size() == 2);
    assert(std::abs(decoded.bays[0].x) < 1e-9);
    assert(std::abs(decoded.bays[0].y) < 1e-9);

    for (const auto& bay : decoded.bays) {
        assert(bay.rotation == 0.0 ||
               bay.rotation == 90.0 ||
               bay.rotation == 180.0 ||
               bay.rotation == 270.0);
    }

    const double expected = expectedQForAllPlaced();
    const double actual = ga.evaluateQ(decoded);
    assert(std::abs(actual - expected) < 1e-9);
    std::cout << "test_decode_and_fitness passed!" << std::endl;
}

int main() {
    test_crossover();
    test_mutation();
    test_decode_and_fitness();
    std::cout << "All GAOrtho tests passed!" << std::endl;
    return 0;
}
