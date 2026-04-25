#include <iostream>
#include <vector>

int main() {
    double wh_area = 10000 * 10000; // 1e8
    
    // Case 0 bay types:
    // id=0, width=800, depth=1200, nLoads=4, price=2000 -> ratio=500, area=960,000, eff = 1920
    // id=1, width=400, depth=600, nLoads=2, price=800 -> ratio=400, area=240,000, eff = 600
    
    double sum_r = 0;
    double sum_a = 0;
    
    // Suppose SA found 12 of type 0.
    sum_r = 12 * 500; // 6000
    sum_a = 12 * 960000; // 11,520,000
    
    double cur_score = sum_r * (wh_area / sum_a) * (wh_area / sum_a);
    std::cout << "12 bays score: " << cur_score << "\n";
    
    // Now try adding one more type 0
    double r = 500;
    double a = 960000;
    double new_score = (sum_r + r) * (wh_area / (sum_a + a)) * (wh_area / (sum_a + a));
    std::cout << "13 bays score: " << new_score << "\n";
    
    return 0;
}
