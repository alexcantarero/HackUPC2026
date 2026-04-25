#include "../src/solvers/greedy.hpp"
#include "../src/core/score.hpp"
#include "../src/core/collision.hpp"
#include <atomic>
#include <cassert>
#include <iostream>
#include <limits>

static int passed = 0, failed = 0;

#define TEST(name, expr) \
    do { if (expr) { std::cout << "  [PASS] " << name << "\n"; ++passed; } \
         else      { std::cout << "  [FAIL] " << name << "\n"; ++failed; } } while(0)

static StaticState makeState() {
    StaticState s;
    s.warehousePolygon = {{0,0},{10000,0},{10000,10000},{0,10000}};
    s.ceilingRegions   = {{0, 5000.0}};
    s.bayTypes.push_back({0, 800.0, 1200.0, 2800.0, 200.0, 4.0, 2000.0});
    return s;
}

void testGreedyPlacesBays() {
    std::cout << "\nGreedy places bays\n";
    StaticState s = makeState();
    GreedySolver solver(s, 42);
    std::atomic<bool> stop{false};
    solver.run(stop);

    const Solution& sol = solver.best();
    TEST("places at least 1 bay",  sol.bays.size() >= 1);
    TEST("score is not max_double", sol.official_score < std::numeric_limits<double>::max());
    TEST("producedBy is 'greedy'",  sol.producedBy == "greedy");
}

void testGreedyBaysAreValid() {
    std::cout << "\nGreedy bays pass collision checks\n";
    StaticState s = makeState();
    GreedySolver solver(s, 42);
    std::atomic<bool> stop{false};
    solver.run(stop);

    const Solution& sol = solver.best();
    for (int i = 0; i < (int)sol.bays.size(); ++i) {
        std::vector<Bay> placed(sol.bays.begin(), sol.bays.begin() + i);
        bool valid = CollisionChecker::isValidPlacement(sol.bays[i], placed, &s);
        TEST("bay " + std::to_string(i) + " is valid", valid);
    }
}

void testGreedyScoreMatchesCompute() {
    std::cout << "\nGreedy score matches computeOfficialScore\n";
    StaticState s = makeState();
    GreedySolver solver(s, 42);
    std::atomic<bool> stop{false};
    solver.run(stop);

    const Solution& sol = solver.best();
    double wh = warehouseArea(s.warehousePolygon);
    double expected = computeOfficialScore(sol.bays, s, wh);
    TEST("score matches computeOfficialScore", std::fabs(sol.official_score - expected) < 1e-6);
}

void testGreedyMultipleTypes() {
    std::cout << "\nGreedy with two bay types\n";
    StaticState s = makeState();
    s.bayTypes.push_back({1, 400.0, 600.0, 2800.0, 100.0, 2.0, 800.0});
    GreedySolver solver(s, 7);
    std::atomic<bool> stop{false};
    solver.run(stop);

    const Solution& sol = solver.best();
    TEST("places at least 1 bay with two types", sol.bays.size() >= 1);
    TEST("score is not max_double with two types",
         sol.official_score < std::numeric_limits<double>::max());
}

int main() {
    std::cout << "=== Greedy Solver Tests ===\n";
    testGreedyPlacesBays();
    testGreedyBaysAreValid();
    testGreedyScoreMatchesCompute();
    testGreedyMultipleTypes();
    std::cout << "\n=== Results: " << passed << " passed, "
              << failed << " failed ===\n";
    return failed > 0 ? 1 : 0;
}
