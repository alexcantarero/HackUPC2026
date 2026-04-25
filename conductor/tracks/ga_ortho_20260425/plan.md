# Implementation Plan: Implement Orthogonal Genetic Algorithm (GA Ortho)

## Phase 1: Genetic Sequence Operators
- [x] Task: Implement Chromosome and Crossover [708f482]
    - [ ] Sub-task: Create GA Ortho class inheriting from base `Algorithm`
    - [ ] Sub-task: Implement Order Crossover (OX1) logic
- [x] Task: Implement Mutation Logic [6d45fc6]
    - [ ] Sub-task: Implement swap mutation
    - [ ] Sub-task: Add unit tests for genetic operators
- [x] Task: Conductor - User Manual Verification 'Phase 1: Genetic Sequence Operators' (Protocol in workflow.md) [8060602]

## Phase 2: Orthogonal Bottom-Left-Fill Decoder
- [x] Task: Implement BLF Decoder
    - [x] Sub-task: Build orthogonal placement loop (sliding left and down)
    - [x] Sub-task: Connect to `CollisionChecker` for validation
- [x] Task: Implement Fitness Evaluation
    - [x] Sub-task: Calculate objective score $Q$ for the decoded sequence
    - [x] Sub-task: Verify score correctness with baseline tests
- [ ] Task: Conductor - User Manual Verification 'Phase 2: Orthogonal Bottom-Left-Fill Decoder' (Protocol in workflow.md)

## Phase 3: Integration and Termination
- [x] Task: Connect to Algorithm Interface
    - [x] Sub-task: Override `run(stop_flag)` in the `Algorithm` interface
    - [x] Sub-task: Ensure the evolution loop checks `stop_flag` for graceful termination
    - [x] Sub-task: Run full tests with multi-threading wrapper
- [ ] Task: Conductor - User Manual Verification 'Phase 3: Integration and Termination' (Protocol in workflow.md)