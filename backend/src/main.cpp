#include "core/types.hpp"
#include "io/parser.hpp"
#include "solvers/algorithm.hpp"
#include "solvers/ga_angle.hpp"
#include "solvers/ga_ortho.hpp"
#include "solvers/greedy.hpp"
#include "solvers/jostle_algorithm.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <memory>
#include <chrono>
#include <atomic>
#include <string>
#include <random>
#include <csignal>
#include <cmath>
// ─── Algorithm stubs (replace with real includes as solvers are implemented) ──
#include "solvers/greedy.hpp"
#include "solvers/sa.hpp"
// #include "solvers/jostle.hpp"
#include "solvers/ga_angle.hpp"
#include "solvers/ga_ortho.hpp"
// #include "solvers/vns.hpp"

static constexpr int    NUM_THREADS   = 6;
static constexpr double TIME_LIMIT_S  = 29.0;
std::atomic<bool> early_exit_signal{false};

struct Config {
    std::string caseDir  = "../data/input/Case0";
    std::string mode     = "parallel";   
    std::string algo     = "greedy";     
};

static Config parseArgs(int argc, char* argv[]) {
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--case"  && i + 1 < argc) cfg.caseDir = argv[++i];
        if (arg == "--mode"  && i + 1 < argc) cfg.mode    = argv[++i];
        if (arg == "--algo"  && i + 1 < argc) cfg.algo    = argv[++i];
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
    if (algoName == "greedy")   return std::make_unique<GreedySolver>(info, seed);
    if (algoName == "greedy")   return std::make_unique<GreedySolver>(info, seed);
    if (algoName == "ga_ortho") return std::make_unique<GAOrtho>(info, seed);
    if (algoName == "ga_angle") return std::make_unique<GAAngle>(info, seed);
    if (algoName == "sa")       return std::make_unique<SimulatedAnnealing>(info, seed);
    // if (algoName == "jostle")   return std::make_unique<JostleAlgorithm>(info, seed, -1); // Run until time limit
    // if (algoName == "vns")      return std::make_unique<Vns>(info, seed);

    std::cerr << "[warn] Unknown algorithm '" << algoName << "'. No solver created.\n";
    return nullptr;
}

void signalHandler(int /*signum*/) {
    std::cout << "\n[INFO] Early exit signal received. Wrapping up threads...\n";
    early_exit_signal = true;
}

int main(int argc, char* argv[]) {
    Config cfg = parseArgs(argc, argv);
    std::signal(SIGINT, signalHandler); 

    StaticState staticData;
    if (!io::parseStaticState(cfg.caseDir, staticData)) return 1;

    std::random_device rd;
    std::vector<std::unique_ptr<Algorithm>> algos;

    if (cfg.mode == "parallel") {
        for (int i = 0; i < NUM_THREADS; ++i) {
            uint64_t seed = rd() ^ (static_cast<uint64_t>(i) << 32);
            auto algo = makeAlgorithm(cfg.algo, staticData, seed);
            if (algo) algos.push_back(std::move(algo));
        }
    } else {
        const std::vector<std::string> portfolio = {"greedy", "ga_ortho", "ga_angle", "jostle", "sa"};
        for (int i = 0; i < (int)portfolio.size(); ++i) {
            uint64_t seed = rd() ^ (static_cast<uint64_t>(i) << 32);
            auto algo = makeAlgorithm(portfolio[i], staticData, seed);
            if (algo) algos.push_back(std::move(algo));
        }
    }

    if (algos.empty()) return 1;

    // 3. Spawn threads
    std::atomic<bool> stop_flag{false}; // atomic means safe to set from main thread and read from algo threads without mutex
    std::vector<std::thread> threads;
    threads.reserve(algos.size());

    for (auto& algo : algos) {
        Algorithm* p = algo.get();
        threads.emplace_back([p, &stop_flag] { p->run(stop_flag); });
    }
    // 4. Wait for time limit (only for anytime algorithms), then signal stop
    bool anyAnytime = std::any_of(algos.begin(), algos.end(),
        [](const auto& a) { return a->isAnytime(); });
    if (anyAnytime){
        std::cout<< "About to start anytime algorithms. Will run for " << TIME_LIMIT_S << " seconds.\n";
        std::this_thread::sleep_for(std::chrono::duration<double>(TIME_LIMIT_S));
    }
    stop_flag = true;

    for (auto& t : threads) t.join();

    const Solution* winner = nullptr;
    for (const auto& algo : algos) {
        const Solution& sol = algo->best();
        // algo_name + timestamp to avoid overwriting solutions when running multiple times
        std::string output_path = "../data/output/solution_" + std::to_string(std::time(nullptr)) + "_" + algo->name() + ".csv";
        io::writeSolution(output_path, sol);
        std::cout << "Algorithm '" << algo->name() << "' produced solution with official score "
                  << sol.official_score << " (training score " << sol.training_score
                  << ") in " << sol.timeTaken << "s with " << sol.bays.size() << " bays.\n";
        if (!sol.bays.empty() &&
            (winner == nullptr || sol.official_score < winner->official_score))
                winner = &sol;
            }

    if (!winner || winner->bays.empty()) {
        std::cerr << "No valid solution found.\n";
        return 1;
    }

    std::cout << "Best solution: score=" << winner->official_score
              << "  (training_score=" << winner->training_score << ")"
              << "  time=" << winner->timeTaken << "s"
              << "  bays=" << winner->bays.size()
              << "  algorithm=" << winner->producedBy << "\n";

    return 0;
}