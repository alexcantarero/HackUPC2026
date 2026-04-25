// #include "ga_angle.hpp"
// #include <algorithm>
// #include <cmath>
// #include <limits>

// namespace {
// void expandBounds(const OBB& obb, double& min_x, double& max_x, double& min_y, double& max_y) {
//     for (int i = 0; i < 4; ++i) {
//         min_x = std::min(min_x, obb.corners[i].x); max_x = std::max(max_x, obb.corners[i].x);
//         min_y = std::min(min_y, obb.corners[i].y); max_y = std::max(max_y, obb.corners[i].y);
//     }
// }
// }  

// GAAngle::GAAngle(const StaticState& info, uint64_t seed) : GeneticAlgorithm(info, seed) {}

// Solution GAAngle::decodeChromosome(const Chromosome& chromosome, std::atomic<bool>& stop_flag) {
//     return decodeContinuousBLF(chromosome, stop_flag);
// }

// Solution GAAngle::decodeContinuousBLF(const Chromosome& chromosome, std::atomic<bool>& stop_flag) {
//     Solution decoded;
//     decoded.producedBy = name();
//     constexpr int kQuasiAnglesPerAnchor = 2; 
//     constexpr int kMaxAnchors = 256;
//     constexpr double kGoldenAngle = 137.50776405003785;

//     if (chromosome.empty()) {
//         calculateMetrics(decoded);
//         return decoded;
//     }

//     std::vector<Point2D> anchors = generateSafeAnchors();
//     SpatialGrid decode_grid(defaultCellSize());
//     bool is_first_bay = true;

//     for (int bay_id : chromosome) {
//         if (stop_flag.load()) break; 

//         if (getBayTypeById(bay_id) == nullptr) continue;

//         // --- BULLETPROOF SORT: Integer Quantization to prevent SIGABRT ---
//         std::sort(anchors.begin(), anchors.end(),[](const Point2D& lhs, const Point2D& rhs) {
//             auto quantize =[](double v) { return static_cast<long long>(std::round(v * 100.0)); };
//             long long q_ly = quantize(lhs.y);
//             long long q_ry = quantize(rhs.y);
//             if (q_ly != q_ry) return q_ly < q_ry;
//             return quantize(lhs.x) < quantize(rhs.x);
//         });

//         bool placed = false;
//         Bay best_candidate{bay_id, 0.0, 0.0, 0.0};

//         for (const auto& anchor : anchors) {
//             std::vector<double> angles = {0.0, 90.0, 180.0, 270.0};
//             const double base_angle = randomAngleDegrees();
//             for (int i = 0; i < kQuasiAnglesPerAnchor; ++i) {
//                 angles.push_back(std::fmod(base_angle + i * kGoldenAngle, 360.0));
//             }

//             for (double angle : angles) {
//                 Bay candidate{bay_id, anchor.x, anchor.y, angle};
//                 if (CollisionChecker::isValidPlacement(candidate, decoded.bays, &info_, &decode_grid)) {
//                     best_candidate = candidate;
//                     placed = true;
//                     break;
//                 }
//             }
//             if (placed) break;
//         }

//         if (!placed) continue;

//         if (is_first_bay) {
//             anchors.clear();
//             for (const auto& point : info_.warehousePolygon) anchors.push_back(point);
//             is_first_bay = false;
//         }

//         decoded.bays.push_back(best_candidate);
//         OBB solid = CollisionChecker::createSolidOBB(best_candidate, &info_);
//         decode_grid.insertBay(static_cast<int>(decoded.bays.size() - 1), solid);

//         OBB gap = CollisionChecker::createGapOBB(best_candidate, &info_);
//         double min_x = solid.corners[0].x, max_x = min_x;
//         double min_y = solid.corners[0].y, max_y = min_y;
//         expandBounds(gap, min_x, max_x, min_y, max_y);
//         expandBounds(solid, min_x, max_x, min_y, max_y);

//         anchors.push_back({max_x, min_y});
//         anchors.push_back({min_x, max_y});

//         if (anchors.size() > static_cast<size_t>(kMaxAnchors)) anchors.resize(kMaxAnchors);
//     }

//     calculateMetrics(decoded);
//     return decoded;
// }