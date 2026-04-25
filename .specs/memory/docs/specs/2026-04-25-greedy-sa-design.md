# Design: Greedy + Simulated Annealing Solvers

**Date:** 2026-04-25  
**Features:** F1 (C++17/20 solver), F2 (parallel execution)

---

## Overview

Implement two solvers that together form the primary optimization pipeline:

1. **GreedySolver** — deterministic BLF (Bottom-Left Fill) constructive pass; fast baseline and SA seed
2. **SimulatedAnnealing** — meta-heuristic that starts from Greedy's output and refines it over the remaining time budget

Both extend the existing `Algorithm` base class (`solvers/algorithm.hpp`).

---

## New Files

```
backend/src/core/score.hpp / score.cpp      ← Q formula, shoelace area
backend/src/solvers/greedy.hpp / greedy.cpp ← GreedySolver
backend/src/solvers/sa.hpp / sa.cpp         ← SimulatedAnnealing
```

---

## Section 1 — Score Utility (`core/score.hpp / score.cpp`)

Stateless free functions, no class needed.

```cpp
// Polygon area via shoelace formula (call once per problem instance)
double warehouseArea(const std::vector<Point2D>& polygon);

// Full Q = (Σ_bay price/loads)² − (Σ_bay width*depth / warehouse_area)
// bay_area per bay = BayType::width * BayType::depth
double computeScore(const std::vector<Bay>& bays,
                    const StaticState& info,
                    double warehouseArea);
```

SA maintains incremental running sums (`sum_ratio`, `sum_area`) so each move recomputes Q in O(1):

```
new_Q = (sum_ratio - old_ratio + new_ratio)²
      − (sum_area  - old_area  + new_area) / wh_area
```

---

## Section 2 — Greedy Solver (`solvers/greedy.hpp / greedy.cpp`)

### Class

```cpp
class GreedySolver : public Algorithm {
public:
    GreedySolver(const StaticState& info, uint64_t seed);
    void run(std::atomic<bool>& stop_flag) override;
    std::string name() const override { return "greedy"; }

    // Exposed for SA to reuse the fill pass without a full run()
    void fillPass();
};
```

### Startup (constructor / first call to fillPass)

- Precompute `wh_area = warehouseArea(info_.warehousePolygon)`
- Sort bay types by `width × depth` descending (largest first)
- Precompute 13 rotation angles: 0°, 15°, 30°, …, 180°

### Candidate Position Set

- Type: `std::set<Point2D>` sorted by `(y, x)` ascending — bottom-left first (BLF priority)
- Initialized with the warehouse AABB bottom-left corner
- After every successful placement, the 4 corners of the new bay's solid OBB are added

### Fill Loop

```
for each position in candidate_set (BLF order):
    for each bay_type (largest → smallest by area):
        for each rotation in [0°, 15°, …, 180°] (shuffled by rng_ for diversity):
            if tryPlace({type_id, pos.x, pos.y, rot}):
                add bay solid OBB corners to candidate_set
                goto next_position   // first-fit: take it and move on
```

### `run()` Behaviour

1. Call `fillPass()` to build the greedy layout into `best_.bays`
2. Compute `best_.score = computeScore(best_.bays, info_, wh_area)`
3. Call `updateBest(best_)` to stamp `producedBy`
4. Returns (does not loop — Greedy is a one-shot constructive pass)

Diversity across 6 parallel threads comes from the shuffled rotation order (seeded differently per thread via `rng_`).

---

## Section 3 — Simulated Annealing (`solvers/sa.hpp / sa.cpp`)

### Class

```cpp
class SimulatedAnnealing : public Algorithm {
public:
    SimulatedAnnealing(const StaticState& info, uint64_t seed);
    void run(std::atomic<bool>& stop_flag) override;
    std::string name() const override { return "sa"; }

private:
    double wh_area_;
    double sum_ratio_;   // Σ price/loads for current layout
    double sum_area_;    // Σ width*depth for current layout
    std::vector<Bay> current_bays_;
    SpatialGrid      current_grid_;

    double currentScore() const;
    void   initSums();
    void   calibrateT0(double& T0) const;
    bool   moveRelocate(double T);
    bool   moveReplaceType(double T);
};
```

### Phase 1 — Greedy Warmup

Inside `run()`, before the SA loop:

1. Instantiate `GreedySolver greedy(info_, /* same seed */ rng_())`
2. Call `greedy.fillPass()`
3. Copy `greedy.best().bays` into `current_bays_`
4. Rebuild `current_grid_` from `current_bays_`
5. Call `initSums()` to compute `sum_ratio_` and `sum_area_`
6. Store greedy result as initial `best_` via `updateBest()`

### T0 Calibration

Before the SA loop, probe 200 random moves (without accepting them), collect `|ΔQ|` values, compute:

```
T0 = mean(|ΔQ|) / ln(2)   // ~50% acceptance rate at start
```

### SA Loop

```cpp
double T = T0;
constexpr double ALPHA = 0.9995;

while (!stop_flag) {
    bool accepted = (rng_() % 2 == 0)
                    ? moveRelocate(T)
                    : moveReplaceType(T);
    if (accepted && currentScore() < best_.score)
        updateBest(makeSolution());   // best_ always kept fresh
    T *= ALPHA;
}
```

### Relocate Move

SA maintains `event_points_: std::vector<Point2D>` — the 4 solid OBB corners of every bay in `current_bays_`, rebuilt after each accepted move (O(N), acceptable since moves are infrequent relative to evaluation cost).

1. Pick random bay `i` from `current_bays_`
2. Remove it; subtract its contribution from `sum_ratio_` and `sum_area_`
3. Pick a random position from `event_points_` (falls back to random point in AABB if empty)
4. Try up to 13 rotations (shuffled); attempt `isValidPlacement` against `current_bays_` + `current_grid_`
5. If valid: insert bay at new position, update sums, update `current_grid_`, rebuild `event_points_`, **accept**
6. If none fit: restore bay `i`, **reject**

### Replace-Type Move

1. Pick random bay `i`; note its `(x, y, rotation)`
2. Remove it; subtract contributions from sums
3. Pick a random *different* bay type `j`
4. Try placing type `j` at the same `(x, y)` with each rotation (shuffled)
5. If valid: insert new bay, update sums, update `current_grid_`, **accept**
6. If none fit: restore bay `i`, **reject**

### Metropolis Criterion

For both move types, after computing `ΔQ = new_score - current_score`:

```
if ΔQ < 0 or uniform(0,1) < exp(-ΔQ / T):  accept
else:                                         reject
```

### Best-State Guarantee

`best_` is updated via `updateBest()` **every time an accepted move improves the score** — not just at the end. When `stop_flag` fires, `algo->best()` always returns the best layout seen so far, never a partial intermediate state.

---

## Integration: `main.cpp` Factory

Uncomment in `makeAlgorithm()`:

```cpp
if (algoName == "greedy") return std::make_unique<GreedySolver>(info, seed);
if (algoName == "sa")     return std::make_unique<SimulatedAnnealing>(info, seed);
```

---

## What Is NOT in Scope

- `writer.hpp/cpp` (solution CSV output) — separate task
- GA, Jostle, VNS solvers — other team members
- Frontend visualizer changes
