#include "State.hpp"
#include <iostream>

int main() {
    StaticState staticData;
    if (staticData.loadFromDirectory("../data/input/Case0")) {
        std::cout << "Successfully loaded static data from Case0." << std::endl;
        std::cout << "Warehouse points: " << staticData.warehousePolygon.size() << std::endl;
        std::cout << "Obstacles: " << staticData.obstacles.size() << std::endl;
        std::cout << "Ceiling regions: " << staticData.ceilingRegions.size() << std::endl;
        std::cout << "Bay types: " << staticData.bayTypes.size() << std::endl;
    } else {
        std::cout << "Failed to load static data." << std::endl;
    }

    // Creating initial state
    State state(&staticData);
    std::cout << "State created." << std::endl;

    return 0;
}
