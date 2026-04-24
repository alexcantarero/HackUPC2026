# Features

## Backend

- **F1** — C++17/20 solver: runs 6 parallel threads for 29s, picks and writes best `solution.csv`
- **F2** — Portfolio mode (`--mode portfolio`): each thread runs a different algorithm (Greedy, GA-ortho, GA-angle, SA, Jostle, VNS)
- **F3** — Parallel mode (`--mode parallel --algo <name>`): all 6 threads run the same non-deterministic algorithm with different random seeds

## Frontend

- **F4** — Case selector: choose which input case (Case0–Case3) to solve
- **F5** — Mode selector: toggle portfolio vs. parallel; if parallel, pick the algorithm
- **F6** — Run & results: trigger the solver, display Q score and total bays placed
- **F7** — Warehouse visualizer: 2D top-down rendering of the warehouse polygon, obstacles, and placed bays (color-coded by type, gap regions visible)

## Bridge

- **F8** — File-based I/O: frontend invokes the C++ binary with CLI flags, reads `solution.csv` on completion to drive the visualizer
