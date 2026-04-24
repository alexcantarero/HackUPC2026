# Product Guidelines

## Core UX & Algorithm Principle
**Algorithmic Efficiency & Visual Delight:** Prioritize nanosecond-level performance in the C++ algorithm to explore maximum states within 30 seconds. For the visualizer, maintain high visual fidelity with smooth animations to clearly illustrate the layout solutions.

## Architecture & Design System
- **C++ Core:** Treat every bay as a composite entity (`Solid` and `Soft` polygons). Do not use `std::mutex` inside core collision loops; use thread-local copies. Implement fast broad-phase (AABB) and narrow-phase (SAT) collision detection.
- **Frontend:** Utilize utility-first styling (TailwindCSS/Similar) for rapid, consistent UI development.

## Development Rules & AI Agent Constraints
- **Performance First (C++):** Precompute trigonometry (`sin`, `cos`) or cache rotation matrices. Immediately update and cache AABBs when moving/rotating bays. Ensure graceful termination before the 30.0s hard limit via `std::atomic<bool> stop_flag`.
- **Strict Documentation:** Always leave detailed JSDoc/TSDoc for React, and standard comments for C++ complex logic.
- **Rigorous Typing:** Ensure complete TypeScript type safety and robust C++ type usage.
- **Modular Code:** Break logic down into small, highly testable files. Decouple sequence logic from placement logic (e.g., Sequence-based GA with BLF decoder).