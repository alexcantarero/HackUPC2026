#include "ga_ortho.hpp"
#include <algorithm>
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

GAOrtho::GAOrtho(const StaticState& info, uint64_t seed)
    : GeneticAlgorithm(info, seed) {
}

Solution GAOrtho::decodeChromosome(const Chromosome& chromosome) {
    return decodeOrthogonalBLF(chromosome);
}

Solution GAOrtho::decodeOrthogonalBLF(const Chromosome& chromosome) {
    Solution decoded;
    decoded.producedBy = name();
    constexpr int kMaxAnchors = 256;

    if (chromosome.empty()) {
        decoded.score = evaluateQ(decoded);
        return decoded;
    }

    std::vector<Point2D> anchors;
    // Seed with all warehouse corners (guaranteed to be inside/on the boundary)
    for (const auto& point : info_.warehousePolygon) {
        anchors.push_back(point);
    }
    // Seed with all obstacle corners
    for (const auto& obs : info_.obstacles) {
        anchors.push_back({obs.x, obs.y});
        anchors.push_back({obs.x + obs.width, obs.y});
        anchors.push_back({obs.x, obs.y + obs.depth});
        anchors.push_back({obs.x + obs.width, obs.y + obs.depth});
    }

    SpatialGrid decode_grid(defaultCellSize());
    for (const auto& bay_id : chromosome) {
        const BayType* bay_type = getBayTypeById(bay_id);
        if (bay_type == nullptr) {
            continue;
        }

        bool found = false;
        Bay best_candidate{bay_id, 0.0, 0.0, 0.0};
        double best_y = std::numeric_limits<double>::max();
        double best_x = std::numeric_limits<double>::max();

        for (const auto& anchor : anchors) {
            for (double angle : {0.0, 90.0, 180.0, 270.0}) {
                Bay candidate{bay_id, anchor.x, anchor.y, angle};
                if (!CollisionChecker::isValidPlacement(
                        candidate,
                        decoded.bays,
                        &info_,
                        &decode_grid)) {
                    continue;
                }

                if (!found ||
                    candidate.y < best_y ||
                    (candidate.y == best_y && candidate.x < best_x)) {
                    found = true;
                    best_candidate = candidate;
                    best_y = candidate.y;
                    best_x = candidate.x;
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
