#include "../src/solvers/algorithm.hpp"
#include "../src/core/types.hpp"
#include <iostream>
#include <limits>
#include <atomic>

// ─── Minimal test runner ──────────────────────────────────────────────────────

static int passed = 0;
static int failed = 0;

#define TEST(name, expr)                                          \
    do {                                                          \
        if (expr) {                                               \
            std::cout << "  [PASS] " << name << "\n";            \
            ++passed;                                             \
        } else {                                                  \
            std::cout << "  [FAIL] " << name << "\n";            \
            ++failed;                                             \
        }                                                         \
    } while (0)

#define SECTION(name) std::cout << "\n" << name << "\n";

// ─── Helpers ─────────────────────────────────────────────────────────────────

static StaticState makeBasicState() {
    StaticState s;
    s.warehousePolygon = {{0,0}, {10000,0}, {10000,10000}, {0,10000}};
    s.ceilingRegions = {{0, 5000}};
    s.bayTypes.push_back({0, 800, 1200, 2800, 200, 4, 2000});
    return s;
}

class TestAlgorithm : public Algorithm {
public:
    TestAlgorithm(const StaticState& info, uint64_t seed) : Algorithm(info, seed) {}

    void run(std::atomic<bool>& stop_flag) override {
        // Mock run
        (void)stop_flag;
    }

    std::string name() const override { return "TestAlgo"; }

    // Expose protected methods
    bool publicTryPlace(const Bay& bay) { return tryPlace(bay); }
    void publicResetLayout(const std::vector<Bay>& bays) { resetLayout(bays); }
    void publicUpdateBest(const Solution& candidate) { updateBest(candidate); }
};

// ─── Tests ───────────────────────────────────────────────────────────────────

void testSolutionDefaults() {
    SECTION("Solution - Defaults");
    Solution s;
    TEST("Score is max initially", s.training_score == std::numeric_limits<double>::max());
    TEST("producedBy is empty initially", s.producedBy == "");
    TEST("Bays list is empty initially", s.bays.empty());
}

void testAlgorithmUpdateBest() {
    SECTION("Algorithm - updateBest");
    StaticState state = makeBasicState();
    TestAlgorithm algo(state, 42);

    Solution initialBest = algo.best();
    TEST("Initial best training_score is max", initialBest.training_score == std::numeric_limits<double>::max());

    Solution candidate1;
    candidate1.training_score = 100.0;
    candidate1.bays.push_back({0, 1000, 1000, 0});
    
    algo.publicUpdateBest(candidate1);
    
    TEST("best is updated when training_score is lower", algo.best().training_score == 100.0);
    TEST("producedBy is set to algorithm name", algo.best().producedBy == "TestAlgo");
    TEST("best has correct bays", algo.best().bays.size() == 1);

    Solution candidate2;
    candidate2.training_score = 150.0; // Worse training_score
    algo.publicUpdateBest(candidate2);
    
    TEST("best is NOT updated when training_score is higher", algo.best().training_score == 100.0);
    TEST("producedBy remains the same", algo.best().producedBy == "TestAlgo");
}

void testAlgorithmTryPlace() {
    SECTION("Algorithm - tryPlace");
    StaticState state = makeBasicState();
    TestAlgorithm algo(state, 123);

    Bay validBay = {0, 1000, 1000, 0};
    TEST("tryPlace returns true for valid bay", algo.publicTryPlace(validBay));
    TEST("best bays updated with valid bay", algo.best().bays.size() == 1);

    // Try placing an overlapping bay
    Bay overlappingBay = {0, 1200, 1000, 0}; 
    TEST("tryPlace returns false for overlapping bay", !algo.publicTryPlace(overlappingBay));
    TEST("best bays not updated with invalid bay", algo.best().bays.size() == 1);
}

void testAlgorithmResetLayout() {
    SECTION("Algorithm - resetLayout");
    StaticState state = makeBasicState();
    TestAlgorithm algo(state, 999);

    Bay b1 = {0, 1000, 1000, 0};
    Bay b2 = {0, 2000, 2000, 0};
    std::vector<Bay> layout = {b1, b2};

    algo.publicResetLayout(layout);
    TEST("resetLayout updates best bays", algo.best().bays.size() == 2);

    // Verify grid was rebuilt by trying an overlapping bay
    Bay overlapping = {0, 1000, 1000, 0};
    TEST("Grid works after resetLayout", !algo.publicTryPlace(overlapping));

    // Valid placement still works
    Bay newValid = {0, 5000, 5000, 0};
    TEST("Valid placement works after resetLayout", algo.publicTryPlace(newValid));
    TEST("Best bays increased to 3", algo.best().bays.size() == 3);
}

int main() {
    std::cout << "=== Algorithm & Solution Tests ===\n";

    testSolutionDefaults();
    testAlgorithmUpdateBest();
    testAlgorithmTryPlace();
    testAlgorithmResetLayout();

    std::cout << "\n=== Results: " << passed << " passed, " << failed << " failed ===\n";
    return failed > 0 ? 1 : 0;
}
