#include "ga_ortho.hpp"
#include "../core/collision.hpp"
#include "../core/types.hpp"
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
    constexpr size_t kMaxAnchors = 500;

    if (chromosome.empty()) {
        calculateMetrics(decoded);
        return decoded;
    }

    Solution best_prefix = decoded;
    double best_prefix_score = std::numeric_limits<double>::max();

    std::vector<Point2D> anchors = generateSafeAnchors();
    SpatialGrid decode_grid(defaultCellSize());

    for (const SpatialGene& gene : chromosome) {
        if (stop_flag.load()) break;

        const BayType* bay_type = getBayTypeById(gene.bay_id);
        if (bay_type == nullptr) continue;

        // Sort ALL anchors by distance to gene target — newest anchors at the
        // tail sort into the right position naturally alongside older ones.
        std::sort(anchors.begin(), anchors.end(),
                  [&gene](const Point2D& lhs, const Point2D& rhs) {
            double d1 = (lhs.x - gene.target_x)*(lhs.x - gene.target_x)
                      + (lhs.y - gene.target_y)*(lhs.y - gene.target_y);
            double d2 = (rhs.x - gene.target_x)*(rhs.x - gene.target_x)
                      + (rhs.y - gene.target_y)*(rhs.y - gene.target_y);

            auto quantize_dist = [](double v) { return static_cast<long long>(std::round(v)); };
            long long q1 = quantize_dist(d1);
            long long q2 = quantize_dist(d2);
            if (q1 != q2) return q1 < q2;

            auto q_pos = [](double v) { return static_cast<long long>(std::round(v * 100.0)); };
            long long q_ly = q_pos(lhs.y), q_ry = q_pos(rhs.y);
            if (q_ly != q_ry) return q_ly < q_ry;
            return q_pos(lhs.x) < q_pos(rhs.x);
        });

        bool placed = false;
        Bay best_candidate{gene.bay_id, 0.0, 0.0, 0.0};

        for (const auto& anchor : anchors) {
            std::vector<double> angles = {0.0, 90.0, 180.0, 270.0};
            std::sort(angles.begin(), angles.end(), [&gene](double a, double b) {
                auto diff = [](double a1, double a2) {
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
        OBB bound = CollisionChecker::createBoundingOBB(best_candidate, &info_);
        decode_grid.insertBay(static_cast<int>(decoded.bays.size() - 1), bound);

        double cur_score = evaluateTraining(decoded.bays);
        if (cur_score < best_prefix_score) {
            best_prefix_score = cur_score;
            best_prefix = decoded;
        }

        addBayAnchors(best_candidate, anchors);

        // Trim by removing from the front
        if (anchors.size() > kMaxAnchors) {
            anchors.erase(anchors.begin(),
                          anchors.begin() + (anchors.size() - kMaxAnchors));
        }
    }

    calculateMetrics(best_prefix);
    return best_prefix;
}