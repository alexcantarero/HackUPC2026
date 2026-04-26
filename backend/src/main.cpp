#include "core/types.hpp"
#include "io/parser.hpp"
#include "solvers/algorithm.hpp"
#include "solvers/ga_angle.hpp"
#include "solvers/ga_ortho.hpp"
#include "solvers/greedy.hpp"
#include "solvers/jostle_algorithm.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <memory>
#include <chrono>
#include <atomic>
#include <string>
#include <random>
#include <csignal>
#include <cmath>
#include <map>
#include <filesystem>
// ─── Algorithm stubs (replace with real includes as solvers are implemented) ──
#include "solvers/greedy.hpp"
#include "solvers/sa.hpp"
// #include "solvers/jostle.hpp"
#include "solvers/ga_angle.hpp"
#include "solvers/ga_ortho.hpp"
// #include "solvers/vns.hpp"

static constexpr int    NUM_THREADS   = 6;
static constexpr int    NUM_SAME_ALGOS = 3;
std::atomic<bool> early_exit_signal{false};

// Change the global time limit to be dynamic
double g_time_limit_s = 29.0;

struct Config {
    std::string caseDir  = "../data/input/Case0";
    std::string mode     = "parallel";   
    std::string algo     = "greedy";     
    // Optional GA overrides from CLI (if *_set is true).
    int pop_size = 0;
    int elite_count = 0;
    int tournament_size = 0;
    int immigrants = 0;
    double swap_base = 0.0, swap_max = 0.0;
    double scramble_base = 0.0, scramble_max = 0.0;
    double replace_base = 0.0, replace_max = 0.0;
    double spatial_base = 0.0, spatial_max = 0.0;

    bool pop_size_set = false;
    bool elite_count_set = false;
    bool tournament_size_set = false;
    bool immigrants_set = false;
    bool swap_base_set = false;
    bool swap_max_set = false;
    bool scramble_base_set = false;
    bool scramble_max_set = false;
    bool replace_base_set = false;
    bool replace_max_set = false;
    bool spatial_base_set = false;
    bool spatial_max_set = false;
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
        if (arg == "--pop_size" && i+1 < argc) { cfg.pop_size = std::stoi(argv[++i]); cfg.pop_size_set = true; }
        if (arg == "--elite_count" && i+1 < argc) { cfg.elite_count = std::stoi(argv[++i]); cfg.elite_count_set = true; }
        if (arg == "--tournament_size" && i+1 < argc) { cfg.tournament_size = std::stoi(argv[++i]); cfg.tournament_size_set = true; }
        if (arg == "--immigrants" && i+1 < argc) { cfg.immigrants = std::stoi(argv[++i]); cfg.immigrants_set = true; }
        
        if (arg == "--swap_base" && i+1 < argc) { cfg.swap_base = std::stod(argv[++i]); cfg.swap_base_set = true; }
        if (arg == "--swap_max" && i+1 < argc) { cfg.swap_max = std::stod(argv[++i]); cfg.swap_max_set = true; }
        if (arg == "--scramble_base" && i+1 < argc) { cfg.scramble_base = std::stod(argv[++i]); cfg.scramble_base_set = true; }
        if (arg == "--scramble_max" && i+1 < argc) { cfg.scramble_max = std::stod(argv[++i]); cfg.scramble_max_set = true; }
        if (arg == "--replace_base" && i+1 < argc) { cfg.replace_base = std::stod(argv[++i]); cfg.replace_base_set = true; }
        if (arg == "--replace_max" && i+1 < argc) { cfg.replace_max = std::stod(argv[++i]); cfg.replace_max_set = true; }
        if (arg == "--spatial_base" && i+1 < argc) { cfg.spatial_base = std::stod(argv[++i]); cfg.spatial_base_set = true; }
        if (arg == "--spatial_max" && i+1 < argc) { cfg.spatial_max = std::stod(argv[++i]); cfg.spatial_max_set = true; }
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
        if (algoName == "ga_ortho") {
            // Tuned from multi-case HPO best result.
            ga_params.population_size = 16;
            ga_params.elite_count = 2;
            ga_params.tournament_size = 8;
            ga_params.immigrants_per_generation = 2;
            ga_params.swap_rate_base = 0.22991921050127154;
            ga_params.swap_rate_max = 0.22991921050127154;
            ga_params.scramble_rate_base = 0.2741085782013291;
            ga_params.scramble_rate_max = 0.2741085782013291;
            ga_params.replace_rate_base = 0.27693061670556535;
            ga_params.replace_rate_max = 0.2956168512770407;
            ga_params.spatial_rate_base = 0.14241835822061896;
            ga_params.spatial_rate_max = 0.6888434806466571;
        } else {
            // Educated default for continuous-angle search: slightly more exploration.
            ga_params.population_size = 20;
            ga_params.elite_count = 2;
            ga_params.tournament_size = 8;
            ga_params.immigrants_per_generation = 2;
            ga_params.swap_rate_base = 0.23;
            ga_params.swap_rate_max = 0.33;
            ga_params.scramble_rate_base = 0.27;
            ga_params.scramble_rate_max = 0.40;
            ga_params.replace_rate_base = 0.28;
            ga_params.replace_rate_max = 0.36;
            ga_params.spatial_rate_base = 0.20;
            ga_params.spatial_rate_max = 0.80;
        }

        if (cfg.pop_size_set) ga_params.population_size = cfg.pop_size;
        if (cfg.elite_count_set) ga_params.elite_count = cfg.elite_count;
        if (cfg.tournament_size_set) ga_params.tournament_size = cfg.tournament_size;
        if (cfg.immigrants_set) ga_params.immigrants_per_generation = cfg.immigrants;

        if (cfg.swap_base_set) ga_params.swap_rate_base = cfg.swap_base;
        if (cfg.swap_max_set) ga_params.swap_rate_max = cfg.swap_max;
        if (cfg.scramble_base_set) ga_params.scramble_rate_base = cfg.scramble_base;
        if (cfg.scramble_max_set) ga_params.scramble_rate_max = cfg.scramble_max;
        if (cfg.replace_base_set) ga_params.replace_rate_base = cfg.replace_base;
        if (cfg.replace_max_set) ga_params.replace_rate_max = cfg.replace_max;
        if (cfg.spatial_base_set) ga_params.spatial_rate_base = cfg.spatial_base;
        if (cfg.spatial_max_set) ga_params.spatial_rate_max = cfg.spatial_max;

        // Keep adaptive ranges valid even if user passes max < base.
        ga_params.swap_rate_max = std::max(ga_params.swap_rate_base, ga_params.swap_rate_max);
        ga_params.scramble_rate_max = std::max(ga_params.scramble_rate_base, ga_params.scramble_rate_max);
        ga_params.replace_rate_max = std::max(ga_params.replace_rate_base, ga_params.replace_rate_max);
        ga_params.spatial_rate_max = std::max(ga_params.spatial_rate_base, ga_params.spatial_rate_max);
        
        std::unique_ptr<GeneticAlgorithm> ga;
        if (algoName == "ga_ortho") ga = std::make_unique<GAOrtho>(info, seed);
        else ga = std::make_unique<GAAngle>(info, seed);
        
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
            const std::string& algoName = portfolio[i];
            int count = (algoName == "greedy") ? 1 : NUM_SAME_ALGOS;
            for (int j = 0; j < count; ++j) {
                uint64_t seed = rd() ^ (static_cast<uint64_t>(i * 10 + j) << 32);
                auto algo = makeAlgorithm(algoName, staticData, seed, cfg);
                if (algo) algos.push_back(std::move(algo));
            }
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

    // ─── Build output folder named after the overall winner ──────────────────
    auto scoreTag = [](double s) -> std::string {
        if (s >= 1e10) return "inf";
        return std::to_string(static_cast<int>(std::round(s / 1000.0))) + "k";
    };

    std::string ts = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    std::string folderPath = "../data/output/" + ts + "_" + winner->producedBy + "_" + scoreTag(winner->official_score);
    std::filesystem::create_directories(folderPath);

    // ─── JSON writer (no external dependency) ────────────────────────────────
    auto writeJson = [](const std::string& path, const Solution& sol) {
        std::ofstream f(path);
        if (!f) return;
        f << "{\n"
          << "  \"algorithm_name\": \"" << sol.producedBy << "\",\n"
          << "  \"score\": " << sol.official_score << ",\n"
          << "  \"training_score\": " << sol.training_score << ",\n"
          << "  \"time_took_to_find_best_sol\": " << sol.timeTaken << ",\n"
          << "  \"number_of_bays\": " << sol.bays.size() << "\n"
          << "}\n";
    };

    // ─── Decide which solutions to persist ───────────────────────────────────
    // parallel mode: same algo ran N times — keep only the overall winner.
    // any other mode: one entry per unique algorithm name, keeping its best run.
    std::map<std::string, const Solution*> toSave;
    if (cfg.mode == "parallel") {
        toSave[winner->producedBy] = winner;
    } else {
        for (const auto& algo : algos) {
            const Solution& sol = algo->best();
            std::cout << "Evaluating solution from [" << algo->name() << "] with score " << sol.official_score << "\n";
            if (sol.bays.empty() || sol.official_score >= std::numeric_limits<double>::max() / 2.0)
                continue;
            auto it = toSave.find(sol.producedBy);
            if (it == toSave.end() || sol.official_score < it->second->official_score)
                toSave[sol.producedBy] = &sol;
        }
    }

    // ─── Write CSV + JSON for every saved solution ────────────────────────────
    for (const auto& [name, sol] : toSave) {
        std::string base = folderPath + "/" + ts + "_" + name + "_" + scoreTag(sol->official_score);
        io::writeSolution(base + ".csv", *sol);
        writeJson(base + ".json", *sol);
        std::cout << "  [" << name << "] score=" << sol->official_score
                  << "  training=" << sol->training_score
                  << "  bays=" << sol->bays.size()
                  << "  → " << base << ".{csv,json}\n";
    }

    std::cout << "Best solution: score=" << winner->official_score
              << "  (training_score=" << winner->training_score << ")"
              << "  time=" << winner->timeTaken << "s"
              << "  bays=" << winner->bays.size()
              << "  algorithm=" << winner->producedBy << "\n";
    std::cout << "Output folder: " << folderPath << "\n";

    return 0;
}