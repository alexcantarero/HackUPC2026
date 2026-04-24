#include "../src/core/collision.hpp"
#include "../src/core/types.hpp"
#include <cmath>
#include <iostream>
#include <cassert>

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

static bool nearlyEqual(double a, double b, double eps = 0.01) {
    return std::fabs(a - b) < eps;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

/** Build a minimal StaticState with one bay type and an L-shaped warehouse. */
static StaticState makeBasicState() {
    StaticState s;
    // Simple 10000x10000 square warehouse
    s.warehousePolygon = {{0,0}, {10000,0}, {10000,10000}, {0,10000}};
    // One ceiling region covering everything
    s.ceilingRegions = {{0, 5000}};
    // One bay type: 800 wide, 1200 deep, 2800 high, 200 gap, 4 loads, price 2000
    s.bayTypes.push_back({0, 800, 1200, 2800, 200, 4, 2000});
    return s;
}

static Bay makeBay(int typeId, double x, double y, double rotation) {
    return {typeId, x, y, rotation};
}

// ─── Tests ───────────────────────────────────────────────────────────────────

void testSolidOBB() {
    SECTION("createSolidOBB");

    StaticState s = makeBasicState();
    Bay bay = makeBay(0, 1000, 2000, 0);
    OBB obb = CollisionChecker::createSolidOBB(bay, &s);

    // At rotation=0: corners should be (1000,2000), (1800,2000), (1800,3200), (1000,3200)
    TEST("BL corner x", nearlyEqual(obb.corners[0].x, 1000));
    TEST("BL corner y", nearlyEqual(obb.corners[0].y, 2000));
    TEST("BR corner x", nearlyEqual(obb.corners[1].x, 1800));
    TEST("BR corner y", nearlyEqual(obb.corners[1].y, 2000));
    TEST("TR corner x", nearlyEqual(obb.corners[2].x, 1800));
    TEST("TR corner y", nearlyEqual(obb.corners[2].y, 3200));
    TEST("TL corner x", nearlyEqual(obb.corners[3].x, 1000));
    TEST("TL corner y", nearlyEqual(obb.corners[3].y, 3200));
    TEST("Center x",    nearlyEqual(obb.center.x, 1400));
    TEST("Center y",    nearlyEqual(obb.center.y, 2600));
}

void testGapOBB() {
    SECTION("createGapOBB");

    StaticState s = makeBasicState();
    Bay bay = makeBay(0, 1000, 2000, 0);
    OBB gap = CollisionChecker::createGapOBB(bay, &s);

    // Gap sits immediately beyond the depth face (y=3200 to y=3400 for gap=200)
    TEST("Gap BL y", nearlyEqual(gap.corners[0].y, 3200));
    TEST("Gap TR y", nearlyEqual(gap.corners[2].y, 3400));
    TEST("Gap same width as solid", nearlyEqual(gap.corners[1].x - gap.corners[0].x, 800));
}

void testGapOBBRotated() {
    SECTION("createGapOBB — rotated 90°");

    StaticState s = makeBasicState();
    Bay bay = makeBay(0, 1000, 2000, 90);
    OBB solid = CollisionChecker::createSolidOBB(bay, &s);
    OBB gap   = CollisionChecker::createGapOBB(bay, &s);

    // After 90° rotation the gap should be adjacent to the solid but not overlapping it
    // We verify by checking that gap vs solid does NOT report intersection
    TEST("Rotated gap does not overlap its own solid", !CollisionChecker::checkOBBvsOBB(solid, gap));
}

void testObstacleOBB() {
    SECTION("createObstacleOBB — uses depth, not height");

    Obstacle obs{500, 500, 300, 400}; // x, y, width, depth
    OBB obb = CollisionChecker::createObstacleOBB(obs);

    TEST("BL x", nearlyEqual(obb.corners[0].x, 500));
    TEST("BL y", nearlyEqual(obb.corners[0].y, 500));
    TEST("TR x", nearlyEqual(obb.corners[2].x, 800));   // 500 + width(300)
    TEST("TR y", nearlyEqual(obb.corners[2].y, 900));   // 500 + depth(400)  ← was broken when using obs.height
    TEST("Center x", nearlyEqual(obb.center.x, 650));
    TEST("Center y", nearlyEqual(obb.center.y, 700));
}

void testOBBvsOBB() {
    SECTION("checkOBBvsOBB");

    StaticState s = makeBasicState();

    // Two clearly overlapping bays (same position)
    Bay a = makeBay(0, 1000, 1000, 0);
    Bay b = makeBay(0, 1000, 1000, 0);
    OBB obbA = CollisionChecker::createSolidOBB(a, &s);
    OBB obbB = CollisionChecker::createSolidOBB(b, &s);
    TEST("Identical bays overlap", CollisionChecker::checkOBBvsOBB(obbA, obbB));

    // Two bays touching but not overlapping (bay b starts where bay a ends)
    Bay c = makeBay(0, 1000, 1000, 0); // ends at x=1800
    Bay d = makeBay(0, 1800, 1000, 0); // starts at x=1800
    OBB obbC = CollisionChecker::createSolidOBB(c, &s);
    OBB obbD = CollisionChecker::createSolidOBB(d, &s);
    TEST("Touching bays do not overlap", !CollisionChecker::checkOBBvsOBB(obbC, obbD));

    // Two clearly separated bays
    Bay e = makeBay(0, 0,    0, 0);
    Bay f = makeBay(0, 5000, 0, 0);
    OBB obbE = CollisionChecker::createSolidOBB(e, &s);
    OBB obbF = CollisionChecker::createSolidOBB(f, &s);
    TEST("Separated bays do not overlap", !CollisionChecker::checkOBBvsOBB(obbE, obbF));
}

void testIsValidPlacement() {
    SECTION("isValidPlacement — basic cases");

    StaticState s = makeBasicState();
    std::vector<Bay> empty;

    // Valid: bay comfortably inside the warehouse
    Bay good = makeBay(0, 1000, 1000, 0);
    TEST("Bay inside warehouse is valid", CollisionChecker::isValidPlacement(good, empty, &s));

    // Invalid: bay starts outside the warehouse
    Bay outside = makeBay(0, -100, 1000, 0);
    TEST("Bay outside warehouse is invalid", !CollisionChecker::isValidPlacement(outside, empty, &s));
}

void testIsValidPlacementObstacle() {
    SECTION("isValidPlacement — obstacle collision");

    StaticState s = makeBasicState();
    s.obstacles.push_back({1200, 1200, 400, 400}); // obstacle in the middle

    std::vector<Bay> empty;
    Bay overlapping = makeBay(0, 1000, 1000, 0); // solid covers 1000-1800 x 1000-2200, intersects obstacle
    TEST("Bay overlapping obstacle is invalid",
         !CollisionChecker::isValidPlacement(overlapping, empty, &s));

    Bay clear = makeBay(0, 5000, 5000, 0);
    TEST("Bay far from obstacle is valid",
         CollisionChecker::isValidPlacement(clear, empty, &s));
}

void testIsValidPlacementCeiling() {
    SECTION("isValidPlacement — ceiling constraint");

    StaticState s = makeBasicState();
    // Bay type 0 has height=2800. Ceiling is 5000 everywhere → fits.
    Bay fits = makeBay(0, 1000, 1000, 0);
    TEST("Bay fits under ceiling", CollisionChecker::isValidPlacement(fits, {}, &s));

    // Now add a low ceiling region that starts at x=0 (height=2000 < bay height 2800)
    s.ceilingRegions = {{0, 2000}};
    TEST("Bay does not fit under low ceiling",
         !CollisionChecker::isValidPlacement(fits, {}, &s));
}

void testIsValidPlacementInterBay() {
    SECTION("isValidPlacement — inter-bay solid vs solid");

    StaticState s = makeBasicState();

    Bay first  = makeBay(0, 1000, 1000, 0);
    Bay second = makeBay(0, 1200, 1000, 0); // overlaps with first

    std::vector<Bay> placed = {first};
    TEST("Overlapping second bay is invalid",
         !CollisionChecker::isValidPlacement(second, placed, &s));

    Bay adjacent = makeBay(0, 1800, 1000, 0); // touches first but does not overlap
    TEST("Adjacent (touching) bay is valid",
         CollisionChecker::isValidPlacement(adjacent, placed, &s));
}

void testGapVsGapIsValid() {
    SECTION("isValidPlacement — gap vs gap must be VALID");

    StaticState s = makeBasicState();

    // Two bays facing each other: first at y=1000 (gap extends to y=3400),
    // second rotated 180° at y=3400 (gap extends downward overlapping first's gap).
    Bay first  = makeBay(0, 1000, 1000,   0);
    Bay second = makeBay(0, 1000, 3400, 180);

    std::vector<Bay> placed = {first};
    TEST("Bays sharing gap space (gap vs gap) is valid",
         CollisionChecker::isValidPlacement(second, placed, &s));
}

void testSolidVsGapIsInvalid() {
    SECTION("isValidPlacement — solid must not enter another bay's gap");

    StaticState s = makeBasicState();

    // first bay: solid 1000-1800 x 1000-2200, gap 1000-1800 x 2200-2400
    Bay first  = makeBay(0, 1000, 1000, 0);
    // second bay: solid starts at y=2200 — inside first's gap region
    Bay second = makeBay(0, 1000, 2200, 0);

    std::vector<Bay> placed = {first};
    TEST("Bay solid entering another bay's gap is invalid",
         !CollisionChecker::isValidPlacement(second, placed, &s));
}

void testIsValidPlacementWithGrid() {
    SECTION("isValidPlacement — with SpatialGrid (broad-phase)");

    StaticState s = makeBasicState();

    // Build a grid with cell size matching bay width
    SpatialGrid grid(800.0);

    Bay first = makeBay(0, 1000, 1000, 0);
    OBB firstSolid = CollisionChecker::createSolidOBB(first, &s);
    grid.insertBay(0, firstSolid);
    std::vector<Bay> placed = {first};

    // Overlapping bay — grid should narrow candidates to {first} and reject
    Bay overlap = makeBay(0, 1200, 1000, 0);
    TEST("Grid: overlapping bay is invalid",
         !CollisionChecker::isValidPlacement(overlap, placed, &s, &grid));

    // Adjacent bay — grid may return {first} as candidate, SAT clears it
    Bay adjacent = makeBay(0, 1800, 1000, 0);
    TEST("Grid: adjacent (touching) bay is valid",
         CollisionChecker::isValidPlacement(adjacent, placed, &s, &grid));

    // Far-away bay — grid returns no candidates, still valid
    Bay farAway = makeBay(0, 7000, 7000, 0);
    TEST("Grid: far-away bay is valid with no candidates checked",
         CollisionChecker::isValidPlacement(farAway, placed, &s, &grid));

    // Grid and brute-force must agree on every case
    TEST("Grid agrees with brute-force on overlapping bay",
         CollisionChecker::isValidPlacement(overlap, placed, &s, nullptr) ==
         CollisionChecker::isValidPlacement(overlap, placed, &s, &grid));
    TEST("Grid agrees with brute-force on adjacent bay",
         CollisionChecker::isValidPlacement(adjacent, placed, &s, nullptr) ==
         CollisionChecker::isValidPlacement(adjacent, placed, &s, &grid));
    TEST("Grid agrees with brute-force on far-away bay",
         CollisionChecker::isValidPlacement(farAway, placed, &s, nullptr) ==
         CollisionChecker::isValidPlacement(farAway, placed, &s, &grid));
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== Collision Engine Tests ===\n";

    testSolidOBB();
    testGapOBB();
    testGapOBBRotated();
    testObstacleOBB();
    testOBBvsOBB();
    testIsValidPlacement();
    testIsValidPlacementObstacle();
    testIsValidPlacementCeiling();
    testIsValidPlacementInterBay();
    testGapVsGapIsValid();
    testSolidVsGapIsInvalid();
    testIsValidPlacementWithGrid();

    std::cout << "\n=== Results: " << passed << " passed, " << failed << " failed ===\n";
    return failed > 0 ? 1 : 0;
}
