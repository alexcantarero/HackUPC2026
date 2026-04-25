# Specification: Refactor Backend Collision and Scaffold Algorithm Tracks

## Overview
This track focuses on assessing the existing C++ backend code, updating project documentation to reflect its "Work In Progress" status, refactoring the collision engine to support both `SpatialGrid` and `AABB` options with `Solid`/`Soft` gap logic, and scaffolding dedicated tracks for each algorithmic solver defined in the specifications.

## Functional Requirements
1. **Documentation Updates:** 
   - Update `GEMINI.md` and `conductor/` files to mark the backend code as 'Partially Implemented (Collision WIP)'.
2. **Collision Engine Refactor (`back_end/src/Collision.hpp` & `.cpp`):**
   - Retain the existing `SpatialGrid` optimization.
   - Add a standard AABB broad-phase optimization option.
   - Implement `Solid` (Rack) and `Soft` (Gap) polygon separation logic to accurately evaluate `Gap vs Gap` overlaps as valid, while failing `Rack vs Rack` and `Rack vs Gap`.
3. **Algorithm Track Scaffolding:**
   - Create separate Conductor tracks (folders, `spec.md`, `plan.md`) for each of the 6 core algorithms defined in the specs:
     1. Constructive Greedy + BLF
     2. Sequence-based GA (Orthogonal)
     3. Sequence-based GA (Continuous)
     4. Simulated Annealing
     5. Iterated Jostle Heuristic
     6. VNS Gap-Maximization

## Out of Scope
- Implementing the actual logic for the 6 solver algorithms (these will be executed in their respective new tracks).
- Implementing the CLI threading/execution mode logic (unless it naturally falls under a general backend refactor track).