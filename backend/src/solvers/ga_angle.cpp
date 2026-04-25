#include "ga_angle.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {

void expandBounds(const OBB& obb,
                  double& min_x,
                  double& max_x,
                  double& min_y,
                  double& max_y) {
    for (int i = 0; i < 4; ++i) {
        min_x = std::min(min_x, obb.corners[i].x);
        max_x = std::max(max_x, obb.corners[i].x);
        min_y = std::min(min_y, obb.corners[i].y);
        max_y = std::max(max_y, obb.corners[i].y);
    }
}

}  // namespace

GAAngle::GAAngle(const StaticState& info, uint64_t seed)
    : GeneticAlgorithm(info, seed) {
}

Solution GAAngle::decodeChromosome(const Chromosome& chromosome) {
    return decodeContinuousBLF(chromosome);
}

Solution GAAngle::decodeContinuousBLF(const Chromosome& chromosome) {
    Solution decoded;
    decoded.producedBy = name();
    constexpr int kQuasiAnglesPerAnchor = 12;
    constexpr int kMaxAnchors = 256;
    constexpr double kGoldenAngle = 137.50776405003785;

    if (chromosome.empty()) {
        decoded.score = evaluateQ(decoded);
        return decoded;
    }

    double min_x = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();
    for (const auto& point : info_.warehousePolygon) {
        min_x = std::min(min_x, point.x);
        min_y = std::min(min_y, point.y);
    }

    std::vector<Point2D> anchors;
    anchors.push_back({min_x, min_y});

    SpatialGrid decode_grid(defaultCellSize());

    for (int bay_id : chromosome) {
        if (getBayTypeById(bay_id) == nullptr) {
            continue;
        }

        bool found = false;
        Bay best_candidate{bay_id, 0.0, 0.0, 0.0};
        double best_y = std::numeric_limits<double>::max();
        double best_x = std::numeric_limits<double>::max();

        for (const auto& anchor : anchors) {
            std::vector<double> angles = {0.0, 90.0, 180.0, 270.0};
            const double base_angle = randomAngleDegrees();
            for (int i = 0; i < kQuasiAnglesPerAnchor; ++i) {
                angles.push_back(
                    std::fmod(base_angle + i * kGoldenAngle, 360.0));
            }

            bool anchor_found = false;
            Bay anchor_best{bay_id, 0.0, 0.0, 0.0};
            double anchor_best_y = std::numeric_limits<double>::max();
            double anchor_best_x = std::numeric_limits<double>::max();

            for (double angle : angles) {
                Bay candidate{bay_id, anchor.x, anchor.y, angle};
                if (!CollisionChecker::isValidPlacement(
                        candidate,
                        decoded.bays,
                        &info_,
                        &decode_grid)) {
                    continue;
                }

                if (!anchor_found ||
                    candidate.y < anchor_best_y ||
                    (candidate.y == anchor_best_y && candidate.x < anchor_best_x)) {
                    anchor_found = true;
                    anchor_best = candidate;
                    anchor_best_y = candidate.y;
                    anchor_best_x = candidate.x;
                }
            }

            if (anchor_found) {
                const std::vector<double> local_deltas = {
                    -12.0, -6.0, -3.0, -1.0, 1.0, 3.0, 6.0, 12.0
                };
                for (double delta : local_deltas) {
                    double refined_angle = std::fmod(anchor_best.rotation + delta, 360.0);
                    if (refined_angle < 0.0) {
                        refined_angle += 360.0;
                    }

                    Bay refined{bay_id, anchor.x, anchor.y, refined_angle};
                    if (!CollisionChecker::isValidPlacement(
                            refined,
                            decoded.bays,
                            &info_,
                            &decode_grid)) {
                        continue;
                    }

                    if (refined.y < anchor_best_y ||
                        (refined.y == anchor_best_y && refined.x < anchor_best_x)) {
                        anchor_best = refined;
                        anchor_best_y = refined.y;
                        anchor_best_x = refined.x;
                    }
                }

                if (!found ||
                    anchor_best.y < best_y ||
                    (anchor_best.y == best_y && anchor_best.x < best_x)) {
                    found = true;
                    best_candidate = anchor_best;
                    best_y = anchor_best.y;
                    best_x = anchor_best.x;
                }
            }
        }

        if (!found) {
            continue;
        }

        decoded.bays.push_back(best_candidate);
        OBB solid = CollisionChecker::createSolidOBB(best_candidate, &info_);
        decode_grid.insertBay(static_cast<int>(decoded.bays.size() - 1), solid);

        OBB gap = CollisionChecker::createGapOBB(best_candidate, &info_);
        double placed_min_x = solid.corners[0].x;
        double placed_max_x = solid.corners[0].x;
        double placed_min_y = solid.corners[0].y;
        double placed_max_y = solid.corners[0].y;
        expandBounds(gap, placed_min_x, placed_max_x, placed_min_y, placed_max_y);
        expandBounds(solid, placed_min_x, placed_max_x, placed_min_y, placed_max_y);

        anchors.push_back({placed_max_x, placed_min_y});
        anchors.push_back({placed_min_x, placed_max_y});

        std::sort(
            anchors.begin(),
            anchors.end(),
            [](const Point2D& lhs, const Point2D& rhs) {
                if (lhs.y != rhs.y) {
                    return lhs.y < rhs.y;
                }
                return lhs.x < rhs.x;
            });
        if (anchors.size() > static_cast<size_t>(kMaxAnchors)) {
            anchors.resize(static_cast<size_t>(kMaxAnchors));
        }
    }

    decoded.score = evaluateQ(decoded);
    return decoded;
}
