#include "core/types.hpp"
#include "io/parser.hpp"
#include "solvers/algorithm.hpp"
#include "solvers/ga_angle.hpp"
#include "solvers/ga_ortho.hpp"
#include <csignal>
#include "solvers/jostle_algorithm.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <memory>
#include <chrono>
#include <atomic>
#include <string>
#include <random>

// ─── Algorithm stubs (replace with real includes as solvers are implemented) ──
// #include "solvers/greedy.hpp"
// #include "solvers/sa.hpp"
// #include "solvers/jostle.hpp"
// #include "solvers/vns.hpp"

static constexpr int    NUM_THREADS   = 6;
static constexpr double TIME_LIMIT_S  = 29.0;

// ─── CLI parsing ─────────────────────────────────────────────────────────────

struct Config {
    std::string caseDir  = "../data/input/Case0";
    std::string mode     = "parallel";   // "parallel" | "portfolio"
    std::string algo     = "greedy";     // used in parallel mode
    std::string outCsv   = "../../data/output/solution_" + std::to_string(std::time(nullptr)) + "_" + algo + ".csv";
};

static Config parseArgs(int argc, char* argv[]) {
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--case"  && i + 1 < argc) cfg.caseDir = argv[++i];
        if (arg == "--mode"  && i + 1 < argc) cfg.mode    = argv[++i];
        if (arg == "--algo"  && i + 1 < argc) cfg.algo    = argv[++i];
        if (arg == "--out"   && i + 1 < argc) cfg.outCsv  = argv[++i];
    }
    return cfg;
}

// ─── Algorithm factory ───────────────────────────────────────────────────────

static std::unique_ptr<Algorithm> makeAlgorithm(
    const std::string& algoName,
    const StaticState& info,
    uint64_t seed)
{
    // Uncomment each line as the solver is implemented:
    // if (algoName == "greedy")   return std::make_unique<GreedySolver>(info, seed);
    if (algoName == "ga_ortho") return std::make_unique<GAOrtho>(info, seed);
    if (algoName == "ga_angle") return std::make_unique<GAAngle>(info, seed);
    // if (algoName == "sa")       return std::make_unique<SimulatedAnnealing>(info, seed);
    if (algoName == "jostle")   return std::make_unique<JostleAlgorithm>(info, seed, 1000); // 1000 max iterations by default
    // if (algoName == "vns")      return std::make_unique<Vns>(info, seed);

    std::cerr << "[warn] Unknown algorithm '" << algoName << "'. No solver created.\n";
    return nullptr;
}

std::atomic<bool> early_exit_signal{false};

void signalHandler(int /*signum*/) {
    std::cout << "\n[INFO] Early exit signal received. Wrapping up threads...\n";
    early_exit_signal = true;
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    Config cfg = parseArgs(argc, argv);

    // 1. Load problem data
    StaticState staticData;
    if (!io::parseStaticState(cfg.caseDir, staticData)) {
        std::cerr << "Failed to load case from: " << cfg.caseDir << "\n";
        return 1;
    }
    std::cout << "Loaded: " << staticData.warehousePolygon.size() << " polygon pts, "
              << staticData.obstacles.size()   << " obstacles, "
              << staticData.ceilingRegions.size() << " ceiling regions, "
              << staticData.bayTypes.size()    << " bay types\n";

    // 2. Build algorithm list
    std::random_device rd;
    std::vector<std::unique_ptr<Algorithm>> algos;

    if (cfg.mode == "parallel") {
        // All threads run the same algorithm with different seeds
        std::cout << "Mode: parallel  algo: " << cfg.algo << "\n";
        for (int i = 0; i < NUM_THREADS; ++i) {
            uint64_t seed = rd() ^ (static_cast<uint64_t>(i) << 32);
            auto algo = makeAlgorithm(cfg.algo, staticData, seed);
            if (algo) algos.push_back(std::move(algo));
        }
    } else {
        // Portfolio: one thread per algorithm
        std::cout << "Mode: portfolio\n";
        const std::vector<std::string> portfolio = {
        //    "greedy", "ga_ortho", "ga_angle", "sa", "jostle", "vns"
              "ga_ortho", "ga_angle"
        };
        for (int i = 0; i < (int)portfolio.size(); ++i) {
            uint64_t seed = rd() ^ (static_cast<uint64_t>(i) << 32);
            auto algo = makeAlgorithm(portfolio[i], staticData, seed);
            if (algo) algos.push_back(std::move(algo));
        }
    }

    if (algos.empty()) {
        std::cerr << "No algorithms available. Implement solvers and uncomment them in makeAlgorithm().\n";
        return 1;
    }

    // 3. Spawn threads
    std::signal(SIGINT, signalHandler); // Catch Ctrl+C
    std::atomic<bool> stop_flag{false};
    std::atomic<int> active_threads{0};
    std::vector<std::thread> threads;
    threads.reserve(algos.size());

    for (auto& algo : algos) {
        active_threads++;
        threads.emplace_back([&algo, &stop_flag, &active_threads] { 
            algo->run(stop_flag); 
            active_threads--; // Decrement when thread voluntarily finishes
        });
    }

    // 4. Polling loop: Wait for time limit, Ctrl+C, or natural completion
    auto start_time = std::chrono::steady_clock::now();
    while (true) {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - start_time;
        
        if (elapsed.count() >= TIME_LIMIT_S) {
            std::cout << "[INFO] 29.0s Hard limit reached.\n";
            break;
        }
        if (early_exit_signal) break;     // User pressed Ctrl+C
        if (active_threads == 0) break;   // All algos finished early on their own

        // Sleep for 50ms then check again (prevents maxing out the main thread's CPU)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Signal all algorithms to break their while loops
    stop_flag = true;
    for (auto& t : threads) t.join();

    // 5. Pick the best solution across all threads
    const Solution* winner = nullptr;
    for (const auto& algo : algos) {
        const Solution& sol = algo->best();
        if (!sol.bays.empty() &&
            (winner == nullptr || sol.score < winner->score))
            winner = &sol;
    }

    if (!winner || winner->bays.empty()) {
        std::cerr << "No valid solution found.\n";
        return 1;
    }

    std::cout << "Best solution: score=" << winner->score
              << "  time=" << winner->timeTaken << "s"
              << "  bays=" << winner->bays.size()
              << "  algorithm=" << winner->producedBy << "\n";

    if (io::writeSolution(cfg.outCsv, *winner)) {
        std::cout << "Successfully saved solution to: " << cfg.outCsv << "\n";
    } else {
        std::cerr << "Failed to save solution to: " << cfg.outCsv << "\n";
    }

    return 0;
}
