# Features

## Backend

- **F1** — C++17/20 solver: runs 6 parallel threads for 29s, picks and writes best `solution.csv`
- **F2** — Parallel execution (`--algo <name>`): all 6 threads run the same user-selected algorithm with different random seeds to maximize solution quality.

## Frontend

- **F3** — Case selector: choose which input case (Case0–Case3) to solve
- **F4** — Algorithm selector: pick the algorithm to execute in parallel
- **F5** — Run & results: trigger the solver, display Q score and total bays placed
- **F6** — Warehouse visualizer: 2D top-down rendering of the warehouse polygon, obstacles, and placed bays (color-coded by type, gap regions visible)

## Bridge

- **F7** — File-based I/O: frontend invokes the C++ binary with CLI flags, reads `solution.csv` on completion to drive the visualizer
