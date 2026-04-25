import optuna
import subprocess
import re
import sys
import argparse
import math

# --- CONFIGURATION ---
SOLVER_PATH = "./bin/solver"
# List all the cases you want to evaluate here!
CASES =[
    "../data/input/Case0",
    "../data/input/Case1",
    "../data/input/Case2",
    "../data/input/Case3"
]
TIME_LIMIT_PER_CASE = 20.0  # 4 cases * 20s = 80 seconds per trial

def suggest_ga_params(trial):
    """Defines the hyperparameter search space for Genetic Algorithms."""
    pop_size = trial.suggest_int("pop_size", 10, 60)
    
    return {
        "--pop_size": pop_size,
        "--elite_count": trial.suggest_int("elite_count", 1, max(1, pop_size // 3)),
        "--tournament_size": trial.suggest_int("tournament_size", 2, 8),
        "--immigrants": trial.suggest_int("immigrants", 0, max(0, pop_size // 4)),
        "--swap_max": trial.suggest_float("swap_max", 0.1, 0.9),
        "--swap_base": trial.suggest_float("swap_base", 0.01, 0.4),
        "--scramble_max": trial.suggest_float("scramble_max", 0.1, 0.9),
        "--scramble_base": trial.suggest_float("scramble_base", 0.01, 0.4),
        "--replace_max": trial.suggest_float("replace_max", 0.1, 0.9),
        "--replace_base": trial.suggest_float("replace_base", 0.01, 0.4),
        "--spatial_base": trial.suggest_float("spatial_base", 0.01, 0.5), # Make sure you add these if your Config accepts them!
        "--spatial_max": trial.suggest_float("spatial_max", 0.1, 0.9)
    }

def suggest_sa_params(trial):
    """Defines the hyperparameter search space for Simulated Annealing."""
    return {
        "--sa_init_t": trial.suggest_float("sa_init_t", 0.01, 10.0, log=True),
        "--sa_cooling": trial.suggest_float("sa_cooling", 0.99, 0.999999, log=True),
        "--sa_w_relocate": trial.suggest_int("sa_w_relocate", 1, 10),
        "--sa_w_replace": trial.suggest_int("sa_w_replace", 1, 10),
        "--sa_w_add": trial.suggest_int("sa_w_add", 5, 20),
        "--sa_w_remove": trial.suggest_int("sa_w_remove", 1, 5)
    }

ALGO_SPACES = {
    "ga_ortho": suggest_ga_params,
    "ga_angle": suggest_ga_params,
    "sa": suggest_sa_params
}

def objective(trial, algo_name):
    params = ALGO_SPACES[algo_name](trial)
    case_scores = []

    print(f"\n[Trial {trial.number}] Running {algo_name}...")

    # Run the solver for EACH case
    for case_dir in CASES:
        cmd =[
            SOLVER_PATH, 
            "--mode", "parallel", 
            "--algo", algo_name, 
            "--case", case_dir, 
            "--time", str(TIME_LIMIT_PER_CASE) 
        ]
        
        for k, v in params.items():
            cmd.extend([k, str(v)])

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=TIME_LIMIT_PER_CASE + 2.0)
            output = result.stdout
            
            if result.returncode != 0:
                print(f"  [Error] Crashed on {case_dir} with code {result.returncode}")
                return float('inf')
                
        except subprocess.TimeoutExpired:
            print(f"  [Warn] Timed out on {case_dir}!")
            return float('inf') 

        match = re.search(r"Best solution: score=([0-9.eE+-]+)", output)
        if match:
            score = float(match.group(1))
            if score == float('inf') or score > 1e12:
                print(f"  [{case_dir}] Invalid/Inf score.")
                return float('inf')
            
            print(f"  [{case_dir}] Score: {score:,.1f}")
            case_scores.append(score)
        else:
            print(f"  [Error] No score parsed for {case_dir}.")
            return float('inf')

    # Aggregate using Geometric Mean (Product of scores ^ (1/N))
    # Using log-sum to prevent float overflow on huge scores
    log_sum = sum(math.log(s) for s in case_scores)
    geom_mean = math.exp(log_sum / len(case_scores))
    
    print(f"  => Geometric Mean Score: {geom_mean:,.1f}")
    return geom_mean


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--algo", type=str, default="ga_ortho", help="Algorithm to optimize")
    parser.add_argument("--trials", type=int, default=100, help="Number of trials")
    args = parser.parse_args()

    if args.algo not in ALGO_SPACES:
        print(f"Error: Algorithm '{args.algo}' not defined.")
        sys.exit(1)

    print(f"Starting Multi-Case HPO for {args.algo}")
    print(f"Targeting Cases: {', '.join(c.split('/')[-1] for c in CASES)}")
    print(f"Time limit per case: {TIME_LIMIT_PER_CASE}s ({TIME_LIMIT_PER_CASE * len(CASES)}s total per trial)\n")
    
    study = optuna.create_study(direction="minimize")
    study.optimize(lambda trial: objective(trial, args.algo), n_trials=args.trials)

    print(f"\n{'='*50}")
    print(f"🥇 BEST MULTI-CASE PARAMETERS FOR {args.algo.upper()} 🥇")
    print(f"{'='*50}")
    print(f"Best Geometric Mean Score: {study.best_value:,.1f}")
    for k, v in study.best_params.items():
        print(f"  --{k} {v}")