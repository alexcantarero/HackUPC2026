#include "parser.hpp"
#include <fstream>
#include <sstream>

static std::vector<double> parseCSVLine(const std::string& line) {
    std::vector<double> result;
    std::stringstream ss(line);
    std::string cell;
    while (std::getline(ss, cell, ',')) {
        try { result.push_back(std::stod(cell)); } catch (...) {}
    }
    return result;
}

namespace io {

bool parseStaticState(const std::string& directoryPath, StaticState& out) {
    // Load warehouse.csv
    {
        std::ifstream file(directoryPath + "/warehouse.csv");
        if (!file.is_open()) return false;
        std::string line;
        while (std::getline(file, line)) {
            auto values = parseCSVLine(line);
            if (values.size() >= 2) {
                out.warehousePolygon.push_back({values[0], values[1]});
            }
        }
    }

    // Load obstacles.csv
    {
        std::ifstream file(directoryPath + "/obstacles.csv");
        if (!file.is_open()) return false;
        std::string line;
        while (std::getline(file, line)) {
            auto values = parseCSVLine(line);
            if (values.size() >= 4) {
                obstacles.push_back({values[0], values[1], values[2], values[3]});
            }
        }
    }

    // Load ceiling.csv
    {
        std::ifstream file(directoryPath + "/ceiling.csv");
        if (!file.is_open()) return false;
        std::string line;
        while (std::getline(file, line)) {
            auto values = parseCSVLine(line);
            if (values.size() >= 2) {
                ceilingRegions.push_back({values[0], values[1]});
            }
        }
    }

    // Load types_of_bays.csv
    {
        std::ifstream file(directoryPath + "/types_of_bays.csv");
        if (!file.is_open()) return false;
        std::string line;
        while (std::getline(file, line)) {
            auto values = parseCSVLine(line);
            if (values.size() >= 7) {
                BayType bt;
                bt.id = static_cast<int>(values[0]);
                bt.width = values[1];
                bt.depth = values[2];
                bt.height = values[3];
                bt.gap = values[4];
                bt.nLoads = values[5];
                bt.price = values[6];
                bayTypes.push_back(bt);

                double maxDim = std::max(bt.width, bt.depth);
                if (maxDim > largestBaySize) {
                    largestBaySize = maxDim;
                }
            }
        }
    }

    return true;
}
