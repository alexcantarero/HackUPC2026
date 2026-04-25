#pragma once

#include <vector>
#include <string>
#include <limits>

// --- STATIC PART ---

struct Point2D {
    double x;
    double y;
};

struct Obstacle {
    double x;
    double y;
    double width;
    double depth;
};

struct BayType {
    int id;
    double width;
    double depth;
    double height;
    double gap; 
    double nLoads; 
    double price; 
};

struct CeilingRegion {
    double x; 
    double height;
};

struct StaticState {
    std::vector<Point2D>       warehousePolygon;
    std::vector<Obstacle>      obstacles;
    std::vector<CeilingRegion> ceilingRegions;
    std::vector<BayType>       bayTypes;
};

// --- DYNAMIC DATA (one per solver thread / candidate solution) ---

struct Bay {
    int    typeId;
    double x;
    double y;
    double rotation; ///< degrees: 0, 90, 180, 270 (or arbitrary for some solvers)
};

struct Solution {
    std::vector<Bay> bays;
    double      score      = std::numeric_limits<double>::max(); ///< Q value; lower is better
    std::string producedBy = "";                                 ///< algorithm name, for logging
};
