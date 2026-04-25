
***

# GEMINI.md: Mecalux Warehouse Optimizer Strategy
**Event:** HackUPC 2026
**Challenge:** 2D Continuous Unequal Area Facility Layout Problem (UA-FLP) / 2D Irregular Bin Packing (2DIBPP)
**Objective:** Minimize the evaluated cost function within a strict 30-second execution limit.

---

## 1. Problem Definition & Parameters

### 1.1 Inputs (CSVs)
*   **Warehouse:** Polygon defined by a set of $(X, Y)$ vertices. **Guaranteed to be axis-aligned.**
*   **Obstacles:** Defined as bounding boxes $(X, Y, \text{Width}, \text{Depth})$.
*   **Ceiling:** Step-function defining the maximum allowable height at any given $X$-coordinate.
*   **Bays (Items to Pack):** Defined by `Id`, `Width`, `Depth`, `Height`, `Gap`, `nLoads`, `Price`.

### 1.2 Constraints & Validation Rules
1.  **Warehouse Containment:** All bays must be completely inside the warehouse walls.
2.  **Obstacle Avoidance:** No bay may intersect with any obstacle.
3.  **Ceiling Height:** The `Height` of a bay must be $\le$ the ceiling limit at the bay's specific $X$-coordinate span.
4.  **The "Gap" Requirement (Crucial):** Every bay requires an empty space in front of it, spanning its `Width` and extending out by `Gap`.
    *   Rack vs. Wall / Obstacle = `COLLISION` (Invalid)
    *   Rack vs. Rack = `COLLISION` (Invalid)
    *   Rack vs. Gap = `COLLISION` (Invalid)
    *   **Gap vs. Gap = `VALID`** (Multiple bays can share the same empty aisle space).

### 1.3 Objective Function
The goal is to minimize $Q$:
$$Q = \left( \sum_{\text{bay}} \frac{\text{price}}{\text{loads}} \right)^ (2 - \frac{\sum_{\text{bay}} \text{area}}{\text{area}_{\text{warehouse}}})$$
*Strategic Note:* We must pack bays with the lowest `price/loads` ratio, and we must pack *as many of them as possible* to maximize the subtracted area term, driving $Q$ as low as possible.

---

## 2. Theoretical Foundations

This challenge sits at the intersection of **2D Irregular Bin Packing (2DIBPP)** and the **Continuous Unequal Area Facility Layout Problem (UA-FLP)**. 

1.  **Continuous vs. Discrete Space:** While discrete grid approaches are easier to code, the continuous nature of this problem (coordinates up to 20,000, free rotation) renders grid-based discretization sub-optimal and memory-heavy. We must optimize in continuous space.
2.  **Collision Detection (SAT vs. NFP):** Academic literature heavily favors the **No-Fit Polygon (NFP)** for 2D nesting. However, NFP generation is computationally complex to implement during a hackathon. Because our shapes are strictly rectangles, we rely on the **Separating Axis Theorem (SAT)**. SAT mathematically guarantees detection of overlapping convex polygons and evaluates in nanoseconds.
3.  **Search Space Topologies:** Direct coordinate mutation via Genetic Algorithms (GA) fails in spatial packing due to catastrophic overlaps. The academic standard is to use GA to evolve a **sequence** of items, and use a deterministic **Bottom-Left-Fill (BLF)** gravity algorithm as the decoder.

---

## 3. Technology Stack & Architecture

To evaluate millions of states within the 30-second limit, the core engine must be written in **C++17/C++20**. Python may only be used as an I/O wrapper to parse CSVs and generate the output file if it speeds up development.

### 3.1 Core Data Structures
Treat every bay as a composite entity of two distinct polygons: `Solid` and `Soft`.
```cpp
struct Point { double x, y; };

struct BayState {
    int id;
    Point center;
    double rotation; // In degrees or radians
    
    // Returns the 4 corners of the physical rack
    std::vector<Point> getRackPolygon() const; 
    
    // Returns the 4 corners of the required gap space
    std::vector<Point> getGapPolygon() const;  
};
```

### 3.2 The Collision Engine (SAT + AABB)
Performance dictates a two-phase collision check:
1.  **Broad Phase (AABB):** Calculate the Axis-Aligned Bounding Box for the bay. If bounding boxes do not intersect, skip SAT.
2.  **Narrow Phase (SAT):** If AABBs intersect, project the 4 vertices of both rectangles onto their 4 normal axes. If there is a clear separation on any axis, there is no collision.

```cpp
bool isValidPlacement(const BayState& candidate, const std::vector<BayState>& layout) {
    // 1. Check Ceiling Constraint
    // 2. Check Warehouse Boundaries
    // 3. Check Obstacles
    // 4. Check Inter-Bay Collisions
    for (const auto& placed : layout) {
        if (checkSAT(candidate.getRackPolygon(), placed.getRackPolygon())) return false;
        if (checkSAT(candidate.getRackPolygon(), placed.getGapPolygon())) return false;
        if (checkSAT(candidate.getGapPolygon(), placed.getRackPolygon())) return false;
        // Gap vs Gap is deliberately omitted!
    }
    return true;
}
```

---

## 4. Parallel Algorithmic Execution

With 6 CPU cores available, we utilize a parallel execution strategy focused on increasing the performance and quality of a single, user-selected algorithm. The solver is invoked with a specific algorithm choice (e.g., `--mode parallel --algo sa`).

Each core runs an independent `<std::thread>` executing the **same** chosen algorithm, initialized with different random seeds (`std::random_device` + thread ID). This maximizes the exploration of the search space for that specific heuristic, yielding higher quality solutions than a single-threaded run. At $T = 29.0$ seconds, a global `std::atomic<bool> stop_flag` triggers, and the main thread harvests the lowest-$Q$ valid layout across all threads.

### Supported Algorithms (User Selectable)

*   **Greedy BLF (`greedy`):** Constructive Greedy + Bottom-Left-Fill. Sorts bays and places them orthogonally. Fast baseline.
*   **GA Orthogonal (`ga_ortho`):** Sequence-based Genetic Algorithm using orthogonal angles.
*   **GA Continuous (`ga_angle`):** Sequence-based Genetic Algorithm using arbitrary/continuous angles.
*   **Simulated Annealing (`sa`):** Relaxes the state space to allow overlaps with heavy penalties, cooling to a valid state.
*   **Iterated Jostle (`jostle`):** Simulates physical shaking by repacking left-to-right and right-to-left.
*   **VNS Gap-Maximization (`vns`):** Variable Neighborhood Search focused on maximizing `Gap vs Gap` overlaps.

*Note:* For deterministic algorithms like `greedy`, running 6 identical threads provides no benefit. The parallel focus is primarily for the non-deterministic metaheuristics.

---

## 5. Implementation Guidelines & Advice

1.  **Fast Trigonometry:** Arbitrary rotation requires `sin()` and `cos()`. Precompute these values for standard angles, or cache the rotation matrices to avoid pipeline stalling on math instructions.
2.  **Thread Synchronization:** Do not use `std::mutex` inside the core collision loops. Pass each thread its own complete copy of the parsed data. No shared writable state means no locking overhead.
3.  **AABB Caching:** Whenever a bay is moved or rotated, immediately update and cache its AABB. The Broad Phase collision check should just be `if (A.max_x < B.min_x || ...)`.
4.  **Graceful Termination:** Ensure your `while(!stop_flag)` checks occur frequently enough that the thread can unwind, serialize its best layout, and return before the 30.0s hard kill from the hackathon judge system.
5.  **Random Seeds:** During development, fix your `std::mt19937` seeds to ensure deterministic debugging. For the final submission, seed them with `std::random_device{}` or the thread ID to ensure maximum search space coverage across the 6 cores.