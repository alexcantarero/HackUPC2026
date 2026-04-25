# Specification: Implement Continuous Genetic Algorithm (GA Angle)

## Overview
This track implements the "Sequence-Based Genetic Algorithm (Continuous)" (`ga_angle`) and reuses the common GA sequence logic introduced for `ga_ortho`.

## Functional Requirements
1. **Chromosome Definition:** Define a chromosome as a permutation of Bay IDs.
2. **Shared Genetic Operators:** Reuse shared OX1 crossover and mutation operators from the GA base class.
3. **Fitness Function / Decoder (BLF):**
   - Implement a continuous-angle BLF decoder.
   - Try orthogonal and random continuous angles during placement.
   - Use the existing AABB + SAT collision engine for validity checks.
   - Compute objective score $Q$ for decoded layouts.
4. **Integration:**
   - Integrate the solver into the multi-threaded execution strategy (`--algo ga_angle`).

## Out of Scope
- Simulated annealing style penalty relaxation.
- Multi-objective tuning beyond objective $Q$.
