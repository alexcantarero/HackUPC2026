# Product Guide

## Initial Concept
A Mecalux Warehouse Optimizer Strategy for HackUPC 2026. The system aims to solve a 2D Continuous Unequal Area Facility Layout Problem (UA-FLP) / 2D Irregular Bin Packing (2DIBPP) challenge. It consists of a high-performance C++ algorithm core and a visually stunning React frontend to display the packing results.

## Primary Goal
Minimize the evaluated cost function ($Q = (\sum (price/loads))^2 - (\sum area / area_{warehouse})$) within a strict 30-second execution limit.

## Key Features
- **High-Performance Algorithm Core:** Written in C++17/C++20, utilizing 6 independent CPU cores running different optimization algorithms (Greedy BLF, GA, Simulated Annealing, VNS).
- **Continuous Space Collision Engine:** Uses Separating Axis Theorem (SAT) with a fast Broad Phase AABB check for nanosecond collision detection.
- **Stunning Frontend Interface:** High-quality React/Vite web visualizer utilizing shaders for a rich user experience and 2D packing visualization.
- **Python I/O Wrapper:** Parses CSV inputs and generates output files, bridging the C++ core and the visualization tools if needed.

## Target Audience
Hackathon judges evaluating algorithm efficiency and performance within the 30-second limit, as well as stakeholders who require a visually appealing representation of the packed warehouse.