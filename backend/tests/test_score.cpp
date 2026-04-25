#include "../src/core/score.hpp"
#include "../src/core/types.hpp"
#include <cmath>
#include <iostream>

static int passed = 0, failed = 0;

#define TEST(name, expr) \
    do { if (expr) { std::cout << "  [PASS] " << name << "\n"; ++passed; } \
         else      { std::cout << "  [FAIL] " << name << "\n"; ++failed; } } while(0)

static bool near(double a, double b, double eps = 1e-6) {
    return std::fabs(a - b) < eps;
}

void testWarehouseArea() {
    std::cout << "\nwarehouseArea\n";
    std::vector<Point2D> sq = {{0,0},{10,0},{10,10},{0,10}};
    TEST("10x10 square", near(warehouseArea(sq), 100.0));

    std::vector<Point2D> tri = {{0,0},{4,0},{0,3}};
    TEST("3-4 right triangle", near(warehouseArea(tri), 6.0));

    TEST("empty polygon", near(warehouseArea({}), 0.0));
}

void testComputeScore() {
    std::cout << "\ncomputeScore\n";
    StaticState s;
    s.warehousePolygon = {{0,0},{10000,0},{10000,10000},{0,10000}};
    // id=0, width=800, depth=1200, height=2800, gap=200, nLoads=4, price=2000
    s.bayTypes.push_back({0, 800.0, 1200.0, 2800.0, 200.0, 4.0, 2000.0});
    double wh = warehouseArea(s.warehousePolygon); // 1e8

    TEST("empty layout score = 0", near(computeScore({}, s, wh), 0.0));

    // One bay: ratio = 2000/4 = 500, area = 800*1200 = 960000
    // Q = pow(500, 2 - 960000/1e8)
    Bay b{0, 0.0, 0.0, 0.0};
    double expected = std::pow(500.0, 2.0 - 960000.0 / wh);
    TEST("one bay score", near(computeScore({b}, s, wh), expected, 1e-4));

    // Two identical bays: ratio_sum=1000, area_sum=1920000
    // Q = pow(1000, 2 - 1920000/1e8)
    Bay b2{0, 2000.0, 0.0, 0.0};
    double expected2 = std::pow(1000.0, 2.0 - 1920000.0 / wh);
    TEST("two bay score", near(computeScore({b, b2}, s, wh), expected2, 1e-4));
}

int main() {
    std::cout << "=== Score Tests ===\n";
    testWarehouseArea();
    testComputeScore();
    std::cout << "\n=== Results: " << passed << " passed, "
              << failed << " failed ===\n";
    return failed > 0 ? 1 : 0;
}
