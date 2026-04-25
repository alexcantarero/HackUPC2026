#include "ga_ortho.hpp"
#include <algorithm>
#include <limits>
#include <cmath>

namespace {
void expandBounds(const OBB& obb, double& min_x, double& max_x, double& min_y, double& max_y) {
    for (int i = 0; i < 4; ++i) {
        min_x = std::min(min_x, obb.corners[i].x); max_x = std::max(max_x, obb.corners[i].x);
        min_y = std::min(min_y, obb.corners[i].y); max_y = std::max(max_y, obb.corners[i].y);
    }
}
}

GAOrtho::GAOrtho(const StaticState& info, uint64_t seed) : GeneticAlgorithm(info, seed) {}

Solution GAOrtho::decodeChromosome(const Chromosome& chromosome, std::atomic<bool>& stop_flag) {
    return decodeOrthogonalBLF(chromosome, stop_flag);
}

Solution GAOrtho::decodeOrthogonalBLF(const Chromosome& chromosome, std::atomic<bool>& stop_flag) {
    Solution decoded;
    decoded.producedBy = name();
    // Allow more dynamic anchors so it doesn't starve when navigating the L-shape
    constexpr int kMaxAnchors = 500;

    if (chromosome.empty()) {
        calculateMetrics(decoded);
        return decoded;
    }

    std::vector<Point2D> anchors = generateSafeAnchors(); 
    SpatialGrid decode_grid(defaultCellSize());

    // CRITICAL FIX: We DO NOT clear the base anchors after the first bay!
    // The base anchors guarantee we can always start new clusters in different
    // parts of the warehouse if the L-shape choke points get blocked.

    for (const SpatialGene& gene : chromosome) {
        if (stop_flag.load()) break; 

        const BayType* bay_type = getBayTypeById(gene.bay_id);
        if (bay_type == nullptr) continue;

        // SPATIAL ANCHOR SNAP: Sort by distance to the Gene's Target (X, Y)
        std::sort(anchors.begin(), anchors.end(), [&gene](const Point2D& lhs, const Point2D& rhs) {
            double d1 = (lhs.x - gene.target_x)*(lhs.x - gene.target_x) + (lhs.y - gene.target_y)*(lhs.y - gene.target_y);
            double d2 = (rhs.x - gene.target_x)*(rhs.x - gene.target_x) + (rhs.y - gene.target_y)*(rhs.y - gene.target_y);
            
            auto quantize_dist =[](double v) { return static_cast<long long>(std::round(v)); };
            long long q1 = quantize_dist(d1);
            long long q2 = quantize_dist(d2);
            if (q1 != q2) return q1 < q2;
            
            auto q_pos =[](double v) { return static_cast<long long>(std::round(v * 100.0)); };
            long long q_ly = q_pos(lhs.y), q_ry = q_pos(rhs.y);
            if (q_ly != q_ry) return q_ly < q_ry;
            return q_pos(lhs.x) < q_pos(rhs.x);
        });

        bool placed = false;
        Bay best_candidate{gene.bay_id, 0.0, 0.0, 0.0};

        for (const auto& anchor : anchors) {
            std::vector<double> angles = {0.0, 90.0, 180.0, 270.0};
            std::sort(angles.begin(), angles.end(), [&gene](double a, double b) {
                auto diff =[](double a1, double a2) {
                    double d = std::fmod(std::abs(a1 - a2), 360.0);
                    return d > 180.0 ? 360.0 - d : d;
                };
                return diff(a, gene.target_rot) < diff(b, gene.target_rot);
            });

            for (double angle : angles) {
                Bay candidate{gene.bay_id, anchor.x, anchor.y, angle};
                if (CollisionChecker::isValidPlacement(candidate, decoded.bays, &info_, &decode_grid)) {
                    best_candidate = candidate;
                    placed = true;
                    break;
                }
            }
            if (placed) break;
        }

        if (!placed) continue;

        decoded.bays.push_back(best_candidate);
        OBB solid = CollisionChecker::createSolidOBB(best_candidate, &info_);
        decode_grid.insertBay(static_cast<int>(decoded.bays.size() - 1), solid);

        OBB gap = CollisionChecker::createGapOBB(best_candidate, &info_);
        double min_x = solid.corners[0].x, max_x = min_x;
        double min_y = solid.corners[0].y, max_y = min_y;
        expandBounds(gap, min_x, max_x, min_y, max_y);
        expandBounds(solid, min_x, max_x, min_y, max_y);

        anchors.push_back({max_x, min_y});
        anchors.push_back({min_x, max_y});

        // Optional: Also add the center of the gap to encourage back-to-back placement
        anchors.push_back(gap.center);

        if (anchors.size() > static_cast<size_t>(kMaxAnchors)) {
            // Keep the earliest anchors (the safe grid) + the newest anchors
            size_t safe_count = info_.warehousePolygon.size() + (info_.obstacles.size() * 4) + 10;
            if (kMaxAnchors > safe_count) {
                anchors.erase(anchors.begin() + safe_count, anchors.begin() + safe_count + (anchors.size() - kMaxAnchors));
            } else {
                anchors.resize(kMaxAnchors);
            }
        }
    }

    calculateMetrics(decoded);
    return decoded;
}