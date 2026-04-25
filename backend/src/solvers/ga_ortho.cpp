#include "ga_ortho.hpp"
#include <algorithm>
#include <unordered_set>

GAOrtho::GAOrtho(const StaticState& info, uint64_t seed)
    : Algorithm(info, seed) {
}

void GAOrtho::run(std::atomic<bool>& stop_flag) {
    // To be implemented in Phase 3
}

GAOrtho::Chromosome GAOrtho::crossoverOX1(const Chromosome& p1, const Chromosome& p2) {
    if (p1.empty() || p1.size() != p2.size()) return p1;
    
    int n = static_cast<int>(p1.size());
    Chromosome child(n, -1);
    
    std::uniform_int_distribution<int> dist(0, n - 1);
    int start = dist(rng_);
    int end = dist(rng_);
    
    if (start > end) std::swap(start, end);
    
    std::unordered_set<int> copied;
    for (int i = start; i <= end; ++i) {
        child[i] = p1[i];
        copied.insert(p1[i]);
    }
    
    int childIdx = (end + 1) % n;
    int p2Idx = (end + 1) % n;
    
    for (int i = 0; i < n; ++i) {
        int candidate = p2[p2Idx];
        if (copied.find(candidate) == copied.end()) {
            child[childIdx] = candidate;
            childIdx = (childIdx + 1) % n;
        }
        p2Idx = (p2Idx + 1) % n;
    }
    
    return child;
}

void GAOrtho::mutateSwap(Chromosome& chromosome) {
    if (chromosome.size() < 2) return;
    std::uniform_int_distribution<int> dist(0, chromosome.size() - 1);
    int idx1 = dist(rng_);
    int idx2 = dist(rng_);
    std::swap(chromosome[idx1], chromosome[idx2]);
}

void GAOrtho::mutateScramble(Chromosome& chromosome) {
    if (chromosome.size() < 2) return;
    std::uniform_int_distribution<int> dist(0, chromosome.size() - 1);
    int start = dist(rng_);
    int end = dist(rng_);
    if (start > end) std::swap(start, end);
    std::shuffle(chromosome.begin() + start, chromosome.begin() + end + 1, rng_);
}
