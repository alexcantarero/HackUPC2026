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

static std::unique_ptr<Algorithm> makeAlgorithm(const std::string& algoName, const StaticState& info, uint64_t seed) {
    if (algoName == "greedy")   return std::make_unique<GreedySolver>(info, seed);
    if (algoName == "ga_ortho") return std::make_unique<GAOrtho>(info, seed);
    if (algoName == "ga_angle") return std::make_unique<GAAngle>(info, seed);
    if (algoName == "jostle")   return std::make_unique<JostleAlgorithm>(info, seed, -1);
    std::cerr << "[warn] Unknown algorithm '" << algoName << "'.\n";
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
        const std::vector<std::string> portfolio = {"greedy", "ga_ortho", "ga_angle", "jostle"};
        for (int i = 0; i < (int)portfolio.size(); ++i) {
            uint64_t seed = rd() ^ (static_cast<uint64_t>(i) << 32);
            auto algo = makeAlgorithm(portfolio[i], staticData, seed);
            if (algo) algos.push_back(std::move(algo));
        }
    }

    if (algos.empty()) return 1;

    std::atomic<bool> stop_flag{false};
    std::atomic<int> active_threads{0};
    std::vector<std::thread> threads;
    threads.reserve(algos.size());

    for (auto& algo : algos) {
        active_threads++;
        threads.emplace_back([&algo, &stop_flag, &active_threads] { 
            algo->run(stop_flag); 
            active_threads--; 
        });
    }

    auto start_time = std::chrono::steady_clock::now();
    while (true) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<double>(now - start_time).count() >= TIME_LIMIT_S) {
            std::cout << "\n[INFO] 29.0s Hard limit reached. Halting.\n";
            break;
        }
        if (early_exit_signal || active_threads == 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    stop_flag = true;
    for (auto& t : threads) t.join();

    const Solution* winner = nullptr;
    for (const auto& algo : algos) {
        const Solution& sol = algo->best();
        if (!sol.bays.empty() &&
            (winner == nullptr || sol.official_score < winner->official_score)) {
            winner = &sol;
        }
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