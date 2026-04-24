# Product Guidelines

## Core UX Principle
**Visual Delight:** Prioritize high visual fidelity, incorporating smooth animations, transitions, and shaders. The application must not only solve a problem but do so with an engaging and visually stunning interface that appeals directly to users and businesses.

## Design System
**Utility Classes (TailwindCSS/Similar):** We utilize utility-first styling to move rapidly without sacrificing custom design elements. The styling must be consistent, scalable, and easy for AI agents to write and maintain on the fly.

## Development Rules & AI Agent Constraints
To ensure fast, error-free development during the hackathon, AI agents (and human developers) must strictly adhere to the following:

- **Strict Documentation:** Always leave detailed JSDoc/TSDoc comments for complex logic, components, and exported functions. Self-documenting code is essential for rapid onboarding and debugging.
- **Rigorous Typing:** Ensure complete TypeScript type safety. The use of `any` or `unknown` is strictly prohibited unless absolutely unavoidable. All props and state must be fully typed.
- **Modular Code:** Break logic down into small, highly testable, single-responsibility files and functions. This reduces cognitive load for agents and makes isolated testing straightforward.