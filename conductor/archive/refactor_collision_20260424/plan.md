# Implementation Plan: Refactor Backend Collision and Scaffold Algorithm Tracks

## Phase 1: Documentation Status Updates
- [ ] Task: Update Project Documentation Status
    - [ ] Sub-task: Update `GEMINI.md` to reflect 'Partially Implemented (Collision WIP)'
    - [ ] Sub-task: Update `conductor/product.md` to reflect current backend status
- [ ] Task: Conductor - User Manual Verification 'Phase 1: Documentation Status Updates' (Protocol in workflow.md)

## Phase 2: Collision Engine Refactor
- [ ] Task: Implement AABB Broadphase and Solid/Soft Logic
    - [ ] Sub-task: Define `Solid` (Rack) and `Soft` (Gap) polygon structures in `State.hpp`/`Collision.hpp`
    - [ ] Sub-task: Implement AABB tree/list functionality in `Collision.cpp`
    - [ ] Sub-task: Modify `SpatialGrid` to handle both `Solid` and `Soft` entities
    - [ ] Sub-task: Update `CollisionChecker` to enforce `Gap vs Gap` as VALID while failing others
- [ ] Task: Conductor - User Manual Verification 'Phase 2: Collision Engine Refactor' (Protocol in workflow.md)

## Phase 3: Algorithm Track Scaffolding
- [ ] Task: Generate Tracks for Solver Algorithms
    - [ ] Sub-task: Generate Conductor artifacts (`spec.md`, `plan.md`, `metadata.json`) for Greedy BLF
    - [ ] Sub-task: Generate artifacts for Sequence-based GA (Orthogonal)
    - [ ] Sub-task: Generate artifacts for Sequence-based GA (Continuous)
    - [ ] Sub-task: Generate artifacts for Simulated Annealing
    - [ ] Sub-task: Generate artifacts for Iterated Jostle Heuristic
    - [ ] Sub-task: Generate artifacts for VNS Gap-Maximization
- [ ] Task: Conductor - User Manual Verification 'Phase 3: Algorithm Track Scaffolding' (Protocol in workflow.md)