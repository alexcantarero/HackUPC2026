#pragma once

#include <vector>
#include <string>

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

class StaticState {
public:
    std::vector<Point2D> warehousePolygon;
    std::vector<Obstacle> obstacles;
    std::vector<CeilingRegion> ceilingRegions;
    std::vector<BayType> bayTypes;

    // Load static characteristics from the CaseX folder (CSV files)
    bool loadFromDirectory(const std::string& directoryPath);
};

// --- DYNAMIC PART ---

struct Bay {
    int typeId;
    double x;
    double y;
    double rotation; // Rotation in degrees
};

class State {
public:
    // Shared static state among all solutions
    const StaticState* staticInfo;
    
    // Dynamic parts
    std::vector<Bay> bays;

    State(const StaticState* staticData) : staticInfo(staticData) {}
    
    // Copy constructor for local search state duplication
    State(const State& other) = default;
};
