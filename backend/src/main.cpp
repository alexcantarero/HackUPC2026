#include "core/types.hpp"
#include "io/parser.hpp"
#include <iostream>

int main() {
    StaticState staticData;
    if (io::parseStaticState("../data/input/Case0", staticData)) {
        std::cout << "Successfully loaded static data from Case0." << std::endl;
        std::cout << "Warehouse points: " << staticData.warehousePolygon.size() << std::endl;
        std::cout << "Obstacles: " << staticData.obstacles.size() << std::endl;
        std::cout << "Ceiling regions: " << staticData.ceilingRegions.size() << std::endl;
        std::cout << "Bay types: " << staticData.bayTypes.size() << std::endl;
    } else {
        std::cout << "Failed to load static data." << std::endl;
    }

    return 0;
}
