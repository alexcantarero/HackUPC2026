import optuna
import subprocess
import re
import sys
import argparse
import math
import ast
from pathlib import Path

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


def get_distributions_for_params(algo_name, params):
    """Build per-trial distributions so imported log trials can be added to Optuna."""
    if algo_name in {"ga_ortho", "ga_angle"}:
        pop_size = int(params["pop_size"])
        return {
            "pop_size": optuna.distributions.IntDistribution(10, 60),
            "elite_count": optuna.distributions.IntDistribution(1, max(1, pop_size // 3)),
            "tournament_size": optuna.distributions.IntDistribution(2, 8),
            "immigrants": optuna.distributions.IntDistribution(0, max(0, pop_size // 4)),
            "swap_max": optuna.distributions.FloatDistribution(0.1, 0.9),
            "swap_base": optuna.distributions.FloatDistribution(0.01, 0.4),
            "scramble_max": optuna.distributions.FloatDistribution(0.1, 0.9),
            "scramble_base": optuna.distributions.FloatDistribution(0.01, 0.4),
            "replace_max": optuna.distributions.FloatDistribution(0.1, 0.9),
            "replace_base": optuna.distributions.FloatDistribution(0.01, 0.4),
            "spatial_base": optuna.distributions.FloatDistribution(0.01, 0.5),
            "spatial_max": optuna.distributions.FloatDistribution(0.1, 0.9),
        }

    if algo_name == "sa":
        return {
            "sa_init_t": optuna.distributions.FloatDistribution(0.01, 10.0, log=True),
            "sa_cooling": optuna.distributions.FloatDistribution(0.99, 0.999999, log=True),
            "sa_w_relocate": optuna.distributions.IntDistribution(1, 10),
            "sa_w_replace": optuna.distributions.IntDistribution(1, 10),
            "sa_w_add": optuna.distributions.IntDistribution(5, 20),
            "sa_w_remove": optuna.distributions.IntDistribution(1, 5),
        }

    raise ValueError(f"Unsupported algorithm: {algo_name}")


def parse_completed_trials_from_log(log_path):
    """Parse Optuna 'Trial ... finished' lines from a text log file."""
    pattern = re.compile(r"Trial\s+\d+\s+finished\s+with\s+value:\s*([0-9.eE+-]+)\s+and\s+parameters:\s*(\{.*\})")
    parsed = []

    with open(log_path, "r") as f:
        for line in f:
            match = pattern.search(line)
            if not match:
                continue
            value = float(match.group(1))
            params = ast.literal_eval(match.group(2))
            parsed.append((value, params))

    return parsed


def trial_signature(value, params):
    rounded_value = round(float(value), 12)
    items = tuple(sorted((str(k), str(v)) for k, v in params.items()))
    return rounded_value, items


def import_trials_to_study(study, algo_name, parsed_trials):
    existing_signatures = {
        trial_signature(t.value, t.params)
        for t in study.trials
        if t.state == optuna.trial.TrialState.COMPLETE and t.value is not None
    }

    imported = 0
    skipped = 0

    for value, params in parsed_trials:
        sig = trial_signature(value, params)
        if sig in existing_signatures:
            skipped += 1
            continue

        distributions = get_distributions_for_params(algo_name, params)
        trial = optuna.trial.create_trial(
            params=params,
            distributions=distributions,
            value=float(value),
            state=optuna.trial.TrialState.COMPLETE,
            user_attrs={"imported_from_log": True},
        )
        study.add_trial(trial)
        existing_signatures.add(sig)
        imported += 1

    return imported, skipped

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
    parser.add_argument("--additional-trials", type=int, default=None, help="Run this many extra trials after loading/importing")
    parser.add_argument("--study-name", type=str, default=None, help="Optuna study name (default: hpo_<algo>)")
    parser.add_argument("--storage", type=str, default="sqlite:///optuna_hpo.db", help="Optuna storage URL")
    parser.add_argument("--resume-log", type=str, default=None, help="Path to nohup/log file with completed Optuna trials to import")
    args = parser.parse_args()

    if args.algo not in ALGO_SPACES:
        print(f"Error: Algorithm '{args.algo}' not defined.")
        sys.exit(1)

    print(f"Starting Multi-Case HPO for {args.algo}")
    print(f"Targeting Cases: {', '.join(c.split('/')[-1] for c in CASES)}")
    print(f"Time limit per case: {TIME_LIMIT_PER_CASE}s ({TIME_LIMIT_PER_CASE * len(CASES)}s total per trial)\n")

    study_name = args.study_name or f"hpo_{args.algo}"
    study = optuna.create_study(
        direction="minimize",
        study_name=study_name,
        storage=args.storage,
        load_if_exists=True,
    )

    if args.resume_log:
        resume_log = Path(args.resume_log)
        if not resume_log.exists():
            print(f"Error: resume log not found: {resume_log}")
            sys.exit(1)

        parsed_trials = parse_completed_trials_from_log(resume_log)
        imported, skipped = import_trials_to_study(study, args.algo, parsed_trials)
        print(f"Imported {imported} trials from log ({skipped} duplicates skipped)")

    n_trials_to_run = args.additional_trials if args.additional_trials is not None else args.trials
    print(f"Study: {study_name} | Existing trials: {len(study.trials)} | Running now: {n_trials_to_run}\n")

    study.optimize(lambda trial: objective(trial, args.algo), n_trials=n_trials_to_run)

    print(f"\n{'='*50}")
    print(f"🥇 BEST MULTI-CASE PARAMETERS FOR {args.algo.upper()} 🥇")
    print(f"{'='*50}")
    print(f"Best Geometric Mean Score: {study.best_value:,.1f}")
    for k, v in study.best_params.items():
        print(f"  --{k} {v}")