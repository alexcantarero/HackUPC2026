# Implementation Plan: Implement Continuous Genetic Algorithm (GA Angle)

## Phase 1: Shared GA Base Refactor
- [x] Task: Introduce Virtual Genetic Algorithm Base
    - [x] Sub-task: Extract OX1 crossover and mutation operators into shared base
    - [x] Sub-task: Extract common GA run loop with stop-flag handling
    - [x] Sub-task: Keep GA Ortho behavior equivalent after refactor
- [ ] Task: Conductor - User Manual Verification 'Phase 1: Shared GA Base Refactor' (Protocol in workflow.md)

## Phase 2: Continuous Decoder Implementation
- [x] Task: Implement Continuous BLF Decoder
    - [x] Sub-task: Add continuous angle candidates per anchor
    - [x] Sub-task: Validate candidates with `CollisionChecker`
- [x] Task: Implement Fitness Evaluation Reuse
    - [x] Sub-task: Reuse shared objective score $Q$ from base GA class
    - [x] Sub-task: Add baseline tests for score and placement decoding
- [ ] Task: Conductor - User Manual Verification 'Phase 2: Continuous Decoder Implementation' (Protocol in workflow.md)

## Phase 3: Runtime Integration
- [x] Task: Connect `ga_angle` to Runtime Factory
    - [x] Sub-task: Wire algorithm creation in `main.cpp`
    - [x] Sub-task: Update build/test targets and execute full backend tests
- [ ] Task: Conductor - User Manual Verification 'Phase 3: Runtime Integration' (Protocol in workflow.md)
