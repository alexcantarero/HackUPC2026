# Specification: Implement Orthogonal Genetic Algorithm (GA Ortho)

## Overview
This track implements the "Sequence-Based Genetic Algorithm (Orthogonal)" (`ga_ortho`) algorithm inheriting from the newly created base `Algorithm` class. It decouples sequence logic from placement logic by using a Bottom-Left-Fill (BLF) decoder restricted to orthogonal angles.

## Functional Requirements
1. **Chromosome Definition:** Define a chromosome as an array (or permutation) of Bay IDs.
2. **Genetic Operators:**
   - **Crossover:** Implement Order-Crossover (OX1) to produce valid permutations of bays.
   - **Mutation:** Implement swap and scramble mutations.
3. **Fitness Function / Decoder (BLF):**
   - Implement an Orthogonal Bottom-Left-Fill gravity decoder.
   - The decoder takes the bay sequence and attempts to pack them tightly at 0, 90, 180, and 270 degrees.
   - It relies on the previously completed AABB + SAT collision engine.
   - The fitness evaluation computes the $Q$ score (minimization objective) of the fully decoded layout.
4. **Integration:** 
   - Integrate the solver into the multi-threaded execution strategy (`--algo ga_ortho`), utilizing the parent Algorithm class interface `Solution solve(input, stop_flag, seed)`.

## Out of Scope
- Continuous arbitrary angle permutations (these will be in the `ga_angle` track).
- Penalty-based state relaxation (handled in Simulated Annealing).
