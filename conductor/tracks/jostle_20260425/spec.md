# Specification: Jostle Algorithm Track

## Overview
Implement a standalone "Jostle" algorithm in the C++ core to compute local search for the 2D Continuous Unequal Area Facility Layout Problem. The algorithm will incrementally optimize warehouse bay configurations.

## Functional Requirements
- **Standalone Algorithm:** The Jostle algorithm must be implemented as its own standalone solver track, extending the base `Algorithm` class.
- **Local Search Operations:** The algorithm must perform the following operations during its local search steps:
  - **Translation:** Move a bay by a delta in X or Y coordinates.
  - **Rotation:** Rotate a bay by a given angle (arbitrary degrees).
  - **Swapping/Mutation:** Swap two bays or change a bay's type.
- **Configurable Parameters:**
  - **Max Iterations:** The algorithm must accept a parameter for the maximum number of local search iterations.
- **Collision Check Integration:** The algorithm must utilize the existing SAT + Broad Phase AABB collision engine (via `SpatialGrid` / `CollisionChecker`) to validate states after applying translation, rotation, or swapping operations.
- **Objective Function Evaluation:** After generating a valid neighborhood state, the algorithm must evaluate the cost function and decide whether to accept the state based on the local search policy.

## Non-Functional Requirements
- High-performance execution, suitable for parallel multi-core processing within the strict 30-second limit.

## Acceptance Criteria
- [ ] A new `JostleAlgorithm` class extending `Algorithm` is created in the C++ core.
- [ ] The algorithm correctly parses and respects the "Max Iterations" configuration parameter.
- [ ] Translation, rotation, and swapping/mutation operators are successfully implemented and mutate the `State`.
- [ ] The algorithm correctly utilizes the spatial grid and SAT checks to reject invalid overlapping/out-of-bounds states.
- [ ] The local search converges to a better or equal cost function value than the initial state within the allowed time limit.