#include "ga_ortho.hpp"
#include <algorithm>
#include <limits>

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
    constexpr int kMaxAnchors = 256;

    if (chromosome.empty()) {
        decoded.score = calculateScore(decoded.bays);
        return decoded;
    }

    std::vector<Point2D> anchors = generateSafeAnchors(); // From parent class! Safe from corner starvation.
    SpatialGrid decode_grid(defaultCellSize());

    for (const auto& bay_id : chromosome) {
        if (stop_flag.load()) break; // HARD BREAK: Instantly yield thread on 29.0s!

        const BayType* bay_type = getBayTypeById(bay_id);
        if (bay_type == nullptr) continue;

        // Keep anchors sorted Bottom-Left
        std::sort(anchors.begin(), anchors.end(),[](const Point2D& lhs, const Point2D& rhs) {
            if (std::abs(lhs.y - rhs.y) > 1e-5) return lhs.y < rhs.y;
            return lhs.x < rhs.x;
        });

        bool placed = false;
        Bay best_candidate{bay_id, 0.0, 0.0, 0.0};

        for (const auto& anchor : anchors) {
            for (double angle : {0.0, 90.0, 180.0, 270.0}) {
                Bay candidate{bay_id, anchor.x, anchor.y, angle};
                if (CollisionChecker::isValidPlacement(candidate, decoded.bays, &info_, &decode_grid)) {
                    best_candidate = candidate;
                    placed = true;
                    break; // Speedup
                }
            }
            if (placed) break; // Speedup
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

        if (anchors.size() > static_cast<size_t>(kMaxAnchors)) anchors.resize(kMaxAnchors);
    }

    decoded.score = calculateScore(decoded.bays);
    return decoded;
}