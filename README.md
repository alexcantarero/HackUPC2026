# 📦 Mecalux Warehouse Optimizer
> **NP-Hard Facility Layout Optimization at Nanosecond Speed.**

An advanced 2D Continuous Unequal Area Facility Layout (UA-FLP) solver designed for the Mecalux Hackathon. This project combines a high-performance C++20 optimization engine with a cinematic React-based 3D visualization dashboard.

---

## 🚀 Quick Start

### 1. Backend Build (C++)
The core engine is written in C++20 for maximum performance within the 30-second execution limit.
```bash
# Compile the solver and test suite
make -C backend
```

### 2. Frontend Build (React + Three.js)
```bash
# Install dependencies and start the dev server
cd frontend
npm install
```

### 3. Run the Optimizer
```bash
cd ..   # from the root directory
./start.sh
```

---

## 🧠 Algorithmic Architecture

This project solves the **2D Continuous Unequal Area Facility Layout Problem (UA-FLP)**. Unlike discrete grid-based solvers, our engine operates in continuous Euclidean space, allowing for arbitrary rotations and micro-adjustments.

### Key Solvers
- **Greedy BLF**: A fast constructive baseline that sorts bay types by efficiency (Area / (Price/Loads)) and packs them using a Bottom-Left-Fill heuristic.
- **Simulated Annealing (SA)**: A stochastic metaheuristic that uses chaotic coordinate jittering, type-swapping, and "reheating" seasons to escape local minima.
- **Genetic Algorithm (GAOrtho & GAAngle)**: Evolves a sequence of "Spatial Genes." 
    - **GAOrtho**: Forces orthogonal alignments (0°, 90°, 180°, 270°).
    - **GAAngle**: Fully continuous rotation optimization.

### Technical Innovations
- **Lamarckian Evolution**: Unique to our implementation—phenotypic improvements discovered during the "Greedy Fill" local search phase are written back into the individual's genotype, allowing the GA to "learn" spatial patterns over generations.
- **Dynamic Gap-Sharing Anchors**: Instead of simple bounding-box packing, our engine calculates geometric normal vectors to project anchors that allow bays to share aisle space ("Gaps") face-to-face, drastically reducing the total area footprint.
- **SAT Collision Engine**: We utilize the **Separating Axis Theorem (SAT)** to evaluate oriented bounding box (OBB) collisions. This provides $O(1)$ intersection checks with nanosecond latency, enabling millions of evaluations per second.

### Replicate our hyperparameter Optimization (Optuna)
```bash
# Run the HPO script to tune mutation and cooling rates
cd backend
source .venv/bin/activate
python hpo.py --algo {ALGO} --trials {N}
```
Where `{ALGO}` is either `ga_ortho`, `ga_angle`, or `sa` and `{N}` is the number of Optuna trials.

---

## 💻 Technology Stack

- **Optimization Core**: C++20 (Multi-threaded with `std::thread`).
- **3D Visualization**: React 19, Three.js, React Three Fiber.
- **Post-Processing**: ACES Filmic Tone Mapping, sRGB Color Space.
- **Machine Learning**: Optuna (Python) for automated parameter tuning.
- **Collision Math**: Custom SAT implementation for rectangular OBBs.

---

## 🎨 Frontend Features

The frontend is not just a viewer—it's a cinematic digital twin of the warehouse.
- **3D Warehouse Visualization**: Real-time rendering of the optimized layout with a perspective camera or top-down view.
- **Contact Shadows**: Dynamic grounding shadows for better spatial perception of layouts.
- **Dollhouse View**: Ceilings and walls automatically fade based on camera height to provide an unobstructed view of the optimized internals.
- **Interactive Solver Panel**: Upload 4 CSVs and trigger the parallel C++ engine directly from the browser.

---
## Authors:
- **Alex Cantarero** - Frontend and Backend Developer 
- **Felipe Martínez** - Frontend and Backend Developer
- **Adrià Cebrián** - Algorithmic Engineer 
- **Marcel Alabart** - Algorithmic Engineer 