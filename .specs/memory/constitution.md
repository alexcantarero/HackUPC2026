# Project Constitution: Mecalux Warehouse Optimizer (HackUPC 2026)

## Problem

Place rack **bays** inside an axis-aligned warehouse polygon to minimize:

```
Q = (Σ_bay price/loads)² - (Σ_bay area / area_warehouse)
```

Lower Q is better. Hard time limit: **30 seconds** per case.

---

## Repository Structure (Monorepo)

```
hackaton_frontend/   ← repo root (despite the name, full monorepo)
  conductor/         ← project docs and AI guidance
  data/input/        ← input CSVs (Case0–Case3)
  frontend/          ← React/Vite app
  backend/           ← C++17/20 solver (TBD: exact location)
```

---

## Tech Stack

| Layer | Decision |
|-------|----------|
| Frontend | React 19 + TypeScript (strict) + Vite |
| Styling | TailwindCSS (utility-first) |
| Backend/Solver | C++17/20, multi-threaded (6 cores) |
| Python | Optional I/O wrapper only |

---

## Domain Rules (Hard Constraints)

1. All bays must be fully inside the warehouse polygon (axis-aligned walls)
2. No bay overlaps with obstacles (rectangular boxes)
3. No bay solid region overlaps another bay's solid region
4. No bay solid region overlaps another bay's **gap region** (aisle in front of the bay)
5. **Gap vs. Gap overlaps are valid** — bays can share aisle space
6. Bay `height` ≤ ceiling height at the bay's X-coordinate span
7. Rotations: 0°, 90°, 180°, 270° (orthogonal); some algorithms may also try arbitrary angles

---

## Backend: Parallel Execution (6 Cores)

The solver runs a single user-selected algorithm across 6 independent threads, each seeded differently. This maximizes the search space exploration to increase the final solution quality. At T=29s a global `stop_flag` fires; the main thread picks the lowest-Q valid result.

### Available Algorithms
- Constructive Greedy + Bottom-Left-Fill (orthogonal) — fast baseline
- Sequence-based Genetic Algorithm (orthogonal angles)
- Sequence-based Genetic Algorithm (continuous/arbitrary angles)
- Simulated Annealing (relaxed overlap-penalized state space)
- Iterated Jostle Heuristic (L→R / R→L repacking + kick)
- VNS maximizing gap-vs-gap sharing

### Backend Rules
- No shared mutable state between threads — each thread owns a full copy of parsed data
- Two-phase collision: AABB broad phase → SAT narrow phase
- Cache AABB on every move/rotation
- Precompute sin/cos for standard angles
- Fixed seeds for development; `std::random_device` / thread ID for submission

---

## Frontend Rules

- **No solver logic in the frontend** — computation stays in C++
- Visualization receives a parsed `Solution` as props; it does not run algorithms
- `any` and `unknown` are prohibited — full TypeScript type safety
- All complex logic and exported functions must have JSDoc/TSDoc comments
- Small, single-responsibility modules — each file does one thing
- Visual quality matters: smooth animations, transitions, and high-fidelity rendering

---

## Backend: Project Structure

```
backend/
  src/
    main.cpp                  ← parse CLI args, load input, spawn 6 threads, wait 29s, write best result
    io/
      parser.hpp / parser.cpp ← reads the 4 CSVs into domain structs
      writer.hpp / writer.cpp ← writes solution CSV (id, x, y, rotation)
    core/
      types.hpp               ← shared structs: Point, Warehouse, Obstacle, BayType, PlacedBay, Solution
      collision.hpp / .cpp    ← AABB broad phase + SAT narrow phase, containment, ceiling check
      score.hpp / .cpp        ← computes Q given a Solution and ProblemInput
    solvers/
      solver.hpp              ← shared interface: Solution solve(input, stop_flag, seed)
      greedy.hpp / .cpp       ← Core 1: BLF greedy baseline
      ga_ortho.hpp / .cpp     ← Core 2: GA with orthogonal angles
      ga_angle.hpp / .cpp     ← Core 3: GA with arbitrary angles
      sa.hpp / .cpp           ← Core 4: Simulated Annealing
      jostle.hpp / .cpp       ← Core 5: Iterated Jostle
      vns.hpp / .cpp          ← Core 6: VNS gap-sharing
  vendor/                     ← header-only libs (dropped in, no install)
  Makefile                    ← targets: make / make run / make clean
  README.md
```

### Build
```
make          # → bin/solver
make run      # runs against data/input/Case0
make clean
```

### Data Flow
```
[4 CSVs] → parser → ProblemInput
                        ↓
           ┌────────────┴────────────┐
        thread1 ... thread6 (each gets own ProblemInput copy)
           └────────────┬────────────┘
                   at T=29s stop_flag=true
                        ↓
           main picks lowest-Q valid Solution
                        ↓
                     writer → solution.csv
```

### Two Execution Modes (CLI flag)

```
./solver --mode portfolio            # 6 threads, each runs a different algorithm
./solver --mode parallel --algo sa   # 6 threads, all run the same algorithm with different seeds
```

- **Portfolio mode**: one thread per algorithm — broadest search coverage
- **Parallel mode**: same solver instantiated 6 times, seeded with `thread_index + std::random_device`. Only meaningful for non-deterministic algorithms (SA, GA, Jostle, VNS). Greedy is deterministic — warn the user if selected.
- Solvers are mode-agnostic: each just receives `(ProblemInput, stop_flag, seed)` and runs until the flag fires.

---

## Frontend ↔ Backend Communication

**File-based.** The frontend writes input CSVs to a known path and invokes the binary. The backend writes `solution.csv` on completion. The frontend reads the output file to render the result.

---

## Open Decisions

- Visualization library (Canvas, SVG, D3, Konva — not yet decided)
- Whether Python wrapper is used or C++ binary is called directly by the frontend
- Exact paths/protocol for the file handoff (which directory, polling vs. inotify vs. fixed wait)
