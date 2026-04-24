
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
$$Q = \left( \sum_{\text{bay}} \frac{\text{price}}{\text{loads}} \right)^2 - \frac{\sum_{\text{bay}} \text{area}}{\text{area}_{\text{warehouse}}}$$
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

## 4. Multi-Core Algorithmic Portfolio

With 6 CPU cores available, we utilize a parallel execution strategy. Each core runs an independent `<std::thread>` executing a different optimization algorithm. At $T = 29.0$ seconds, a global `std::atomic<bool> stop_flag` triggers, and the main thread harvests the lowest-$Q$ valid layout.

### Core 1: Constructive Greedy + BLF (The Baseline)
*   **Theory:** Walls and obstacles are strictly axis-aligned. Therefore, orthogonal packing ($0^\circ, 90^\circ, 180^\circ, 270^\circ$) yields near-optimal density without the computational cost of arbitrary angles.
*   **Mechanism:** Sort bays ascending by `(price/loads)` and descending by `area`. Place them sequentially using a Bottom-Left-Fill (slide down and left until collision).
*   **Purpose:** Guarantees a highly competitive, valid solution within 0.5 seconds in case metaheuristics fail to converge.

### Core 2: Sequence-Based Genetic Algorithm (Orthogonal)
*   **Theory:** Decouples placement logic from sequence logic.
*   **Mechanism:** The chromosome is an array of Bay IDs. The GA applies order-crossover (OX1) and swap mutations. The fitness function passes the sequence to the BLF decoder (restricted to orthogonal angles). 
*   **Purpose:** Explores global permutations of bay placement rapidly.

### Core 3: Sequence-Based Genetic Algorithm (Continuous Angles)
*   **Mechanism:** Same as Core 2, but the BLF decoder is allowed to attempt 8 fixed angles ($0^\circ, 45^\circ, 90^\circ, 135^\circ...$) and arbitrary offsets. 
*   **Purpose:** Slower evaluation, but capable of finding non-intuitive diagonal interlocking arrangements (like the $70^\circ$ example in the prompt).

### Core 4: Simulated Annealing (Relaxed State Space)
*   **Theory:** Working strictly in a valid state space causes algorithms to get trapped in local minima (unable to move a bay past an obstacle). We relax the space to allow overlaps, heavily penalizing them in the objective function.
*   **Evaluation:** $H = Q + (W_{\text{overlap}} \times \text{OverlapArea}) + (W_{\text{bounds}} \times \text{OutOfBoundsArea})$
*   **Neighborhood Moves:**
    *   *Macro:* Teleport bay to random $(X,Y)$.
    *   *Medium:* Shift by Gaussian delta $(\sigma = 1000)$.
    *   *Micro:* Shift by 1-50 units to flush against walls.
    *   *Rotation:* $80\%$ chance to snap orthogonally, $20\%$ chance for continuous random angle.
*   **Cooling Schedule:** Start hot (accepting heavily overlapping states to bypass obstacles), cooling exponentially so that the final 5 seconds require $H = Q$ (zero overlap penalty).

### Core 5: Iterated Jostle Heuristic
*   **Theory:** Simulates physical shaking.
*   **Mechanism:** 
    1. Pack pieces Left-to-Right.
    2. Sort current layout by $X$-coordinate.
    3. Repack Right-to-Left.
    4. Apply a "Kick" (remove 3 random bays and re-insert them) if the score stagnates for $N$ iterations.
*   **Purpose:** Extremely effective at naturally sliding bays past each other into highly compressed, tight clusters.

### Core 6: Gap-Maximization Variable Neighborhood Search (VNS)
*   **Theory:** Since `Gap vs Gap` overlaps are free, maximizing gap overlaps leaves more warehouse area for solid racks.
*   **Mechanism:** Starts with the output of Core 1. Uses VNS to specifically attempt to rotate and translate bays so that they face each other (overlapping their gap polygons). Evaluates local neighborhoods iteratively ($N_1$: Rotate $180^\circ$, $N_2$: Translate across aisle, etc.).

---

## 5. Implementation Guidelines & Advice

1.  **Fast Trigonometry:** Arbitrary rotation requires `sin()` and `cos()`. Precompute these values for standard angles, or cache the rotation matrices to avoid pipeline stalling on math instructions.
2.  **Thread Synchronization:** Do not use `std::mutex` inside the core collision loops. Pass each thread its own complete copy of the parsed data. No shared writable state means no locking overhead.
3.  **AABB Caching:** Whenever a bay is moved or rotated, immediately update and cache its AABB. The Broad Phase collision check should just be `if (A.max_x < B.min_x || ...)`.
4.  **Graceful Termination:** Ensure your `while(!stop_flag)` checks occur frequently enough that the thread can unwind, serialize its best layout, and return before the 30.0s hard kill from the hackathon judge system.
5.  **Random Seeds:** During development, fix your `std::mt19937` seeds to ensure deterministic debugging. For the final submission, seed them with `std::random_device{}` or the thread ID to ensure maximum search space coverage across the 6 cores.