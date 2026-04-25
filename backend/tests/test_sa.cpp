#include "../src/solvers/sa.hpp"
#include "../src/core/collision.hpp"
#include "../src/core/score.hpp"
#include "../src/solvers/greedy.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <limits>
#include <thread>

static int passed = 0, failed = 0;

#define TEST(name, expr) \
    do { if (expr) { std::cout << "  [PASS] " << name << "\n"; ++passed; } \
         else      { std::cout << "  [FAIL] " << name << "\n"; ++failed; } } while(0)

static StaticState makeState() {
    StaticState s;
    s.warehousePolygon = {{0,0},{10000,0},{10000,10000},{0,10000}};
    s.ceilingRegions   = {{0, 5000.0}};
    s.bayTypes.push_back({0, 800.0, 1200.0, 2800.0, 200.0, 4.0, 2000.0});
    s.bayTypes.push_back({1, 400.0,  600.0, 2800.0, 100.0, 2.0,  800.0});
    return s;
}

// Runs SA for 'seconds' seconds, returns the best solution.
static Solution runSA(const StaticState& s, double seconds, uint64_t seed = 42) {
    std::atomic<bool> stop{false};
    SimulatedAnnealing sa(s, seed);

    std::thread timer([&stop, seconds]() {
        std::this_thread::sleep_for(
            std::chrono::duration<double>(seconds));
        stop = true;
    });
    sa.run(stop);
    timer.join();
    return sa.best();
}

void testSAProducesValidSolution() {
    std::cout << "\nSA produces valid solution\n";
    StaticState s = makeState();
    Solution sol = runSA(s, 2.0);

    TEST("SA places at least 1 bay",   sol.bays.size() >= 1);
    TEST("SA score is not max_double",  sol.score < std::numeric_limits<double>::max());
    TEST("SA producedBy is 'sa'",       sol.producedBy == "sa");
}

void testSABaysAreValid() {
    std::cout << "\nSA bays pass collision checks\n";
    StaticState s = makeState();
    Solution sol = runSA(s, 2.0);

    for (int i = 0; i < (int)sol.bays.size(); ++i) {
        std::vector<Bay> placed(sol.bays.begin(), sol.bays.begin() + i);
        bool valid = CollisionChecker::isValidPlacement(sol.bays[i], placed, &s);
        TEST("SA bay " + std::to_string(i) + " is valid", valid);
    }
}

void testSAScoreConsistentWithBays() {
    std::cout << "\nSA best_.score matches computeScore of its own bays\n";
    StaticState s = makeState();
    Solution sol = runSA(s, 2.0);

    double wh       = warehouseArea(s.warehousePolygon);
    double expected = computeScore(sol.bays, s, wh);
    TEST("SA score matches computeScore", std::fabs(sol.score - expected) < 1e-6);
}

int main() {
    std::cout << "=== SA Solver Tests ===\n";
    testSAProducesValidSolution();
    testSABaysAreValid();
    testSAScoreConsistentWithBays();
    std::cout << "\n=== Results: " << passed << " passed, "
              << failed << " failed ===\n";
    return failed > 0 ? 1 : 0;
}