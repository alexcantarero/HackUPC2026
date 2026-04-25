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
std::atomic<bool> early_exit_signal{false};

// Change the global time limit to be dynamic
double g_time_limit_s = 29.0;

struct Config {
    std::string caseDir  = "../data/input/Case0";
    std::string mode     = "parallel";   
    std::string algo     = "greedy";     
    // We will parse GA params here as well!
    int pop_size = 24;
    int elite_count = 4;
    int tournament_size = 3;
    int immigrants = 2;
    double swap_base = 0.25, swap_max = 0.55;
    double scramble_base = 0.10, scramble_max = 0.35;
    double replace_base = 0.15, replace_max = 0.40;
};

static Config parseArgs(int argc, char* argv[]) {
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--case"  && i + 1 < argc) cfg.caseDir = argv[++i];
        if (arg == "--mode"  && i + 1 < argc) cfg.mode    = argv[++i];
        if (arg == "--algo"  && i + 1 < argc) cfg.algo    = argv[++i];
        if (arg == "--time"  && i + 1 < argc) g_time_limit_s = std::stod(argv[++i]);
        
        // Parse Optuna GA params
        if (arg == "--pop_size" && i+1 < argc) cfg.pop_size = std::stoi(argv[++i]);
        if (arg == "--elite_count" && i+1 < argc) cfg.elite_count = std::stoi(argv[++i]);
        if (arg == "--tournament_size" && i+1 < argc) cfg.tournament_size = std::stoi(argv[++i]);
        if (arg == "--immigrants" && i+1 < argc) cfg.immigrants = std::stoi(argv[++i]);
        
        if (arg == "--swap_base" && i+1 < argc) cfg.swap_base = std::stod(argv[++i]);
        if (arg == "--swap_max" && i+1 < argc) cfg.swap_max = std::stod(argv[++i]);
        if (arg == "--scramble_base" && i+1 < argc) cfg.scramble_base = std::stod(argv[++i]);
        if (arg == "--scramble_max" && i+1 < argc) cfg.scramble_max = std::stod(argv[++i]);
        if (arg == "--replace_base" && i+1 < argc) cfg.replace_base = std::stod(argv[++i]);
        if (arg == "--replace_max" && i+1 < argc) cfg.replace_max = std::stod(argv[++i]);
    }
    return cfg;
}

// NOTE: Update the while loop condition in main() to use g_time_limit_s instead of TIME_LIMIT_S
// if (elapsed.count() >= g_time_limit_s) { break; }

// ─── Algorithm factory ───────────────────────────────────────────────────────

static std::unique_ptr<Algorithm> makeAlgorithm(
    const std::string& algoName,
    const StaticState& info,
    uint64_t seed,
    const Config& cfg = {})
{
    // Uncomment each line as the solver is implemented:
    if (algoName == "greedy")   return std::make_unique<GreedySolver>(info, seed);
    if (algoName == "greedy")   return std::make_unique<GreedySolver>(info, seed);
    if (algoName == "sa")       return std::make_unique<SimulatedAnnealing>(info, seed);
    // if (algoName == "jostle")   return std::make_unique<JostleAlgorithm>(info, seed, -1); // Run until time limit
    // if (algoName == "vns")      return std::make_unique<Vns>(info, seed);

    if (algoName == "ga_ortho" || algoName == "ga_angle") {
        GeneticAlgorithm::GAParams ga_params;
        ga_params.population_size = cfg.pop_size;
        ga_params.elite_count = cfg.elite_count;
        ga_params.tournament_size = cfg.tournament_size;
        ga_params.immigrants_per_generation = cfg.immigrants;
        ga_params.swap_rate_base = cfg.swap_base;
        ga_params.swap_rate_max = std::max(cfg.swap_base, cfg.swap_max); // Prevent max < base
        ga_params.scramble_rate_base = cfg.scramble_base;
        ga_params.scramble_rate_max = std::max(cfg.scramble_base, cfg.scramble_max);
        ga_params.replace_rate_base = cfg.replace_base;
        ga_params.replace_rate_max = std::max(cfg.replace_base, cfg.replace_max);
        
        std::unique_ptr<GeneticAlgorithm> ga;
        if (algoName == "ga_ortho") ga = std::make_unique<GAOrtho>(info, seed);
        //else ga = std::make_unique<GAAngle>(info, seed);
        
        ga->setParams(ga_params);
        return ga;
    }

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
        std::cout<< "About to start anytime algorithms. Will run for " << g_time_limit_s << " seconds.\n";
        std::this_thread::sleep_for(std::chrono::duration<double>(g_time_limit_s));
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

    if (!winner || winner->bays.empty() || winner->official_score >= std::numeric_limits<double>::max() / 2.0) {
        std::cerr << "No valid solution found within the time limit.\n";
        return 1;
    }

    // --- SAFE CASTING TO PREVENT CSV FILENAME CRASH ---
    std::string score_str;
    if (winner->official_score >= 1e10) {
        score_str = "inf";
    } else {
        score_str = std::to_string(static_cast<int>(std::round(winner->official_score / 1000.0))) + "k";
    }
    
    std::string ts = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    std::string finalCsvName = "../../data/output/" + ts + "_" + winner->producedBy + "_" + score_str + ".csv";

    std::cout << "Best solution: score=" << winner->official_score
              << "  (training_score=" << winner->training_score << ")"
              << "  time=" << winner->timeTaken << "s"
              << "  bays=" << winner->bays.size()
              << "  algorithm=" << winner->producedBy << "\n";

    if (io::writeSolution(finalCsvName, *winner)) {
        std::cout << "Successfully saved solution to: " << finalCsvName << "\n";
    } else {
        std::cerr << "Failed to save solution to: " << finalCsvName << "\n";
    }

    return 0;
}