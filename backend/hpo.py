import optuna
import subprocess
import re
import sys
import argparse

# --- CONFIGURATION ---
SOLVER_PATH = "./bin/solver"
CASE_DIR = "../data/input/Case0" # Assumes hpo.py is run from inside `backend/`
TIME_LIMIT = 30.0  # Slightly longer (5s) so the GA has time to finish Gen 0

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
        "--replace_base": trial.suggest_float("replace_base", 0.01, 0.4)
    }

ALGO_SPACES = {
    "ga_ortho": suggest_ga_params,
    "ga_angle": suggest_ga_params,
}

def objective(trial, algo_name):
    params = ALGO_SPACES[algo_name](trial)

    cmd =[
        SOLVER_PATH, 
        "--mode", "parallel", 
        "--algo", algo_name, 
        "--case", CASE_DIR, 
        "--time", str(TIME_LIMIT) 
    ]
    
    for k, v in params.items():
        cmd.extend([k, str(v)])

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=TIME_LIMIT + 2.0)
        output = result.stdout
        
        # ERROR CHECKING: If it crashed, print WHY it crashed!
        if result.returncode != 0:
            print(f"\n[Error] Solver crashed with return code {result.returncode}")
            print(f"Stderr: {result.stderr.strip()}")
            return float('inf')
            
    except subprocess.TimeoutExpired:
        print("[Warn] Process timed out entirely! The C++ code ignored the stop_flag.")
        return float('inf') 

    match = re.search(r"Best solution: score=([0-9.eE+-]+)", output)
    if match:
        score = float(match.group(1))
        if score == float('inf') or score > 1e10:
            return float('inf')
        return score
    else:
        print(f"\n[Error] Could not find 'Best solution: score=' in output.")
        print(f"Output was: {output.strip()}")
        return float('inf')

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--algo", type=str, default="ga_ortho", help="Algorithm to optimize")
    parser.add_argument("--trials", type=int, default=100, help="Number of trials")
    args = parser.parse_args()

    if args.algo not in ALGO_SPACES:
        print(f"Error: Algorithm '{args.algo}' not defined.")
        sys.exit(1)

    print(f"Starting Optuna HPO for {args.algo} ({args.trials} trials, {TIME_LIMIT}s per trial)...")
    
    study = optuna.create_study(direction="minimize")
    study.optimize(lambda trial: objective(trial, args.algo), n_trials=args.trials)

    print(f"\n{'='*40}")
    print(f"🥇 BEST PARAMETERS FOR {args.algo.upper()} 🥇")
    print(f"{'='*40}")
    print(f"Best Score: {study.best_value}")
    for k, v in study.best_params.items():
        print(f"  --{k} {v}")