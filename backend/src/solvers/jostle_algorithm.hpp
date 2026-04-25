#pragma once

#include "algorithm.hpp"

class JostleAlgorithm : public Algorithm {
public:
    JostleAlgorithm(const StaticState& info, uint64_t seed, int maxIterations);

    void run(std::atomic<bool>& stop_flag) override;
    std::string name() const override;

    // Mutation operators
    bool translateBay(std::vector<Bay>& state, int bayIndex, double deltaX, double deltaY);
    bool rotateBay(std::vector<Bay>& state, int bayIndex, double deltaAngle);
    bool swapBays(std::vector<Bay>& state, int bayIndex1, int bayIndex2);
    bool changeBayType(std::vector<Bay>& state, int bayIndex, int newTypeId);
    bool addRandomBay(std::vector<Bay>& state);
    bool removeRandomBay(std::vector<Bay>& state);

    // Helpers
    void greedyPlacement(std::vector<Bay>& state);
    bool isValidState(const std::vector<Bay>& state);
    bool isBayValidInState(const std::vector<Bay>& state, int bayIndex);

    // Objective function
    double evaluateQ(const std::vector<Bay>& state) const;

private:
    int maxIterations_;
    static double polygonArea(const std::vector<Point2D>& polygon);
};
