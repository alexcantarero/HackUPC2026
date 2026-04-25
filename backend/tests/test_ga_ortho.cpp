#include "../src/solvers/ga_ortho.hpp"
#include "../src/core/types.hpp"
#include <iostream>
#include <cassert>
#include <algorithm>

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

int main() {
    test_crossover();
    test_mutation();
    std::cout << "All GAOrtho tests passed!" << std::endl;
    return 0;
}
