# Pune Tourist Route Planner

> **A Personalized Multi-Objective Route Planning System with Custom Data Structures in C++17 and Interactive Browser Frontend.**

[![C++17](https://img.shields.io/badge/Language-C%2B%2B17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/Build-CMake%20%7C%20g%2B%2B-brightgreen.svg)](https://cmake.org)
[![Status](https://img.shields.io/badge/Course_Project-Data_Structures-orange.svg)]()

---

## 1. Project Overview & Scope

The **Pune Tourist Route Planner** is a course project designed to demonstrate the real-world application of core **Data Structures** and **Greedy/Graph Algorithms** in C++17. The scope is strictly **Pune city**, featuring 10 prominent cultural and heritage landmarks:

1. **SW** – Shaniwar Wada (*heritage*)
2. **LM** – Lal Mahal (*heritage*)
3. **DG** – Dagadusheth Halwai Ganapati Temple (*temple, culture*)
4. **PT** – Pataleshwar Cave Temple (*heritage, temple*)
5. **KM** – Raja Dinkar Kelkar Museum (*museum, culture*)
6. **SB** – Sarasbaug (*garden, temple*)
7. **PH** – Parvati Hill (*nature, temple*)
8. **PG** – Pu La Deshpande Udyan (*garden, nature*)
9. **AK** – Aga Khan Palace (*heritage, museum*)
10. **RZ** – Rajiv Gandhi Zoological Park, Katraj (*nature, wildlife*)

Instead of offering a standard single-objective shortest path, this planner computes **personalized multi-criterion routes** balancing:
- **Distance** ($d$ km)
- **Congestion-Adjusted Travel Time** ($t = b \times (1 + \text{traffic}/10)$ min)
- **Scenic Quality** ($s \in [0, 10]$)
- **Crowd Exposure** ($c \in [0, 10]$)
- **Interest Alignment** (matches against tourist interest categories)

It recalculates routes dynamically when roads are blocked or traffic surges, performs **BFS reachability validation**, and maintains an **Undo Stack** to restore prior graph states.

---

## 2. Data Structure Mapping & Complexity Table

Every data structure has been **hand-written from scratch** (no standard container substitutes like `std::priority_queue` or `std::unordered_map`) as templates with viva-oriented documentation.

| # | Data Structure | Header File | Where Used in Project | Time Complexity | Space Complexity | Concept Demonstrated |
|---|---|---|---|---|---|---|
| **1** | **Adjacency List Graph** | `src/ds/Graph.h` | Represents Pune road network; shared mutable `Road` table | Neighbor scan: $O(\text{deg}(u))$<br>Road lookup: $O(1)$ avg | $O(V + E)$ | Two-way edges point to single shared mutable record so dynamic updates affect both directions atomically. Supports parallel edges & directed flags. |
| **2** | **Min-Heap Priority Queue** | `src/ds/MinHeap.h` | Powers Dijkstra's shortest path algorithm | Push: $O(\log V)$<br>Pop: $O(\log V)$<br>Decrease-Key: $O(\log V)$<br>Peek: $O(1)$ | $O(V)$ | Complete binary tree in array buffer. Position index array `pos_[id]` provides $O(1)$ index lookup enabling true $O(\log V)$ `decrease_key()`. |
| **3** | **Hash Map (Separate Chaining)** | `src/ds/HashMap.h` | `place_id -> node_idx` & `road_id -> Road` table | Insert/Lookup/Erase:<br>Avg: $O(1)$<br>Worst: $O(N)$ | $O(N + M)$ | Custom FNV-1a 64-bit hash, linked collision chains, 0.75 load factor threshold triggering automatic $2\times$ capacity doubling and rehashing. |
| **4** | **Dynamic Array** | `src/ds/DynArray.h` | Path sequences, legs, candidate route lists, edge lists | `push_back`: Amortized $O(1)$<br>Random access: $O(1)$ | $O(N)$ | Geometric expansion (factor 2), contiguous memory layout maximizing L1/L2 CPU cache line hits. |
| **5** | **Stack** | `src/ds/Stack.h` | Dynamic update undo history | Push: $O(1)$<br>Pop: $O(1)$<br>Peek: $O(1)$ | $O(K)$ | LIFO ADT storing inverse mutation records (`UndoRecord`) for atomic rollbacks of road blocks, crowd spikes, and traffic changes. |
| **6** | **Queue** | `src/ds/Queue.h` | BFS reachability checks & Simulation event queue | Push: $O(1)$<br>Pop: $O(1)$<br>Front: $O(1)$ | $O(N)$ | FIFO ADT implemented as a dynamic circular ring buffer with zero per-node allocation overhead. |
| **7** | **Merge Sort & Insertion Sort** | `src/ds/Sort.h` | Candidate route ranking by balanced cost $\to$ time $\to$ distance $\to$ road ID | Merge Sort: $O(N \log N)$<br>Insertion Sort: $O(N^2)$ (best $O(N)$) | $O(N)$ | Stable sorting preserving relative order of equivalent candidates. Insertion sort cutoff for tiny sub-partitions. |
| **8** | **Disjoint Set (Union-Find)** | `src/ds/UnionFind.h` | Connected components analysis after road closures | Find / Unite:<br>Amortized $O(\alpha(V))$ | $O(V)$ | Path compression + union-by-rank. Immediately detects if a closure partitions Pune into disconnected sub-graphs. |
| **9** | **Trie (Prefix Tree)** | `src/ds/Trie.h` | Instant autocomplete search for attractions | Insert: $O(L)$<br>Prefix Search: $O(P + K)$ | $O(\sum L \cdot |\Sigma|)$ | Fast prefix matching as user types attraction names into search input boxes. |

---

## 3. Algorithm Formulations

### 3.1 Edge Cost & Objective Modes

For an edge with distance $d$ (km), base time $b$ (min), traffic $tr \in [0, 10]$, scenic quality $s \in [0, 10]$, crowd $c \in [0, 10]$:
$$\text{effective travel time } t = b \times \left(1 + \frac{tr}{10}\right)$$
$$\text{scenic quality} = \frac{s}{10}, \quad \text{crowd exposure} = \frac{c}{10}$$
$$\text{mismatch}(v) = \begin{cases} 0 & \text{if no interests given or } v \text{ matches any interest} \\ 1 & \text{otherwise} \end{cases}$$

The 5 optimization modes are:
1. **Shortest**: $\text{Cost} = d$
2. **Fastest**: $\text{Cost} = t$
3. **Scenic (Low Scenery Penalty)**: $\text{Cost} = d \times (0.10 + 1.0 - \text{scenic quality})$
4. **Least Crowded (Low Crowd Exposure)**: $\text{Cost} = t \times (0.10 + \text{crowd exposure})$
5. **Balanced Multi-Objective**:
$$\text{Cost} = w_d \left(\frac{d}{10}\right) + w_t \left(\frac{t}{30}\right) + w_s \left(\frac{d}{10}\right)(1 - \text{scenic quality}) + w_c \left(\frac{t}{30}\right) \text{crowd} + w_p \left(\frac{d}{10}\right) \text{mismatch}(v)$$

*All edge weights are strictly non-negative*, satisfying Dijkstra's optimality condition.

### 3.2 Demo Index
To provide a normalized score between 0 and 100 for user presentation:
$$\text{Demo Index} = \frac{100}{1 + \text{Balanced Cost}}$$

### 3.3 Personalized Tour Planner (Greedy & Heuristic)
1. **Conflict Resolution**: Validates that no mandatory (`mustVisit`) stop belongs to an avoided category.
2. **Mandatory Ordering (Nearest-Next + Bounded 2-Opt)**:
   - Uses a greedy **Nearest-Neighbor** heuristic from origin through all mandatory stops to destination.
   - Refines stop order using bounded **2-Opt local search** (reversing sub-segments $[i \dots j]$ to eliminate route crossings).
   - *Academic Note*: Traveling Salesperson Problem (TSP) is NP-hard; Nearest-Neighbor + 2-Opt is an established polynomial-time heuristic approximation.
3. **Greedy Optional Attraction Selection**:
   - Computes benefit-to-visit-time ratio: $R = \frac{\text{matching interest count}}{\text{visit minutes}}$.
   - Greedily tests best tour insertion position within user's `max_time_min` budget.
4. **Leg Stitching**: Connects consecutive stops using Dijkstra.

### 3.4 Diverse Alternative Generation
1. Generates routes across all 5 primary modes.
2. Runs bounded edge-penalty reruns (up to 20 reruns), applying penalties to steer exploration.
3. Re-evaluates ground-truth metrics on original costs.
4. Filters out candidates exceeding a $1.75\times$ detour ratio over shortest distance.
5. Ranks via hand-written **Merge Sort**.
6. Filters candidates with Jaccard road overlap $> 70\%$ to guarantee route diversity.

---

## 4. How to Build & Run

### Prerequisites
- Modern C++17 compiler (`g++` 8+ or MSVC or Clang)
- `CMake` 3.14+ (optional, one-line g++ also supported)
- Windows / Linux / macOS (Windows uses `-lws2_32`)

### Option A: One-Line `g++` Build Command (Fastest)

From the `pune_route_planner` folder:
```bash
# Compile the main web server executable
g++ -std=c++17 -Wall -Wextra -O2 src/main.cpp src/planner/Models.cpp src/planner/Scoring.cpp src/planner/Dijkstra.cpp src/planner/Alternatives.cpp src/planner/Tour.cpp src/planner/Dynamic.cpp src/util/Json.cpp -lws2_32 -o pune_route_planner.exe

# Run the server on port 8080
./pune_route_planner.exe --port 8080
```

To run the complete data structures test suite with one line:
```bash
g++ -std=c++17 -Wall -Wextra -O2 tests/test_all.cpp src/planner/Models.cpp src/planner/Scoring.cpp src/planner/Dijkstra.cpp src/planner/Alternatives.cpp src/planner/Tour.cpp src/planner/Dynamic.cpp src/util/Json.cpp -o test_all.exe
./test_all.exe data/pune_demo.json
```

### Option B: CMake Build & CTest

```bash
# From pune_route_planner directory:
cmake -B build -S .
cmake --build build --config Release

# Run automated tests via ctest
ctest --test-dir build --output-on-failure

# Launch the server
./build/pune_route_planner.exe
```

Open your browser to: **`http://127.0.0.1:8080`**

---

## 5. Step-by-Step UI Verification & Screenshot Guide

1. **Initial View**:
   - The schematic canvas renders all 10 Pune attractions with crowd-colored corridors.
   - The banner prominently displays: *"Real attraction names; all numeric attributes and road corridors are synthetic classroom examples, not verified navigation or live conditions."*
2. **Point-to-Point Calculation**:
   - Select Origin: `SW - Shaniwar Wada`, Destination: `SB - Sarasbaug`.
   - Click **Calculate Best Route**.
   - Verify:
     - Shortest Mode: 3.2 km via corridor `e10`.
     - Fastest Mode: 22.8 min via corridors `e04` (Pataleshwar bypass) and `e11`.
3. **Dijkstra Visualizer**:
   - Click **▶ Play** in the toolbar. Watch as nodes light up in purple sequence as Dijkstra settles each node out of the binary MinHeap.
4. **Dynamic Mutations & Undo**:
   - Click corridor `e10` on the map or click **Simulate Event**.
   - Road `e10` becomes dashed red with an $\times$ mark.
   - The **Diff Banner** appears showing: `Rerouted! Changed roads: +2 / -1 | Distance: +0.1 km | Time: +3.8 min`.
   - Click **Undo (1)**: The change is popped from the Stack and corridor `e10` is restored.
5. **Personalized Tour Planning**:
   - Origin: `SW`, Destination: `RZ (Katraj Zoo)`.
   - Check `DG` (Dagadusheth) and `PH` (Parvati Hill) under Must-Visit stops.
   - Enter Time Budget: `200` min.
   - Click **Calculate Best Route**: Displays a 4-stop tour itinerary detailing travel and visiting minutes per stop.
6. **DS Telemetry**:
   - Click **📊 DS Telemetry** in the header to view live metrics on heap operations, hash bucket load factor, undo stack depth, and DSU connected components.

---

## 6. Verified Test Suite Output

Running `./test_all.exe data/pune_demo.json` produces:
```text
===========================================
   RUNNING DATA STRUCTURES TEST SUITE      
===========================================
[RUN] test_dyn_array...
  -> PASSED
[RUN] test_min_heap (order + decrease-key)...
  -> PASSED
[RUN] test_hash_map (insert, lookup, collision, rehash)...
  -> PASSED
[RUN] test_stack_queue...
  -> PASSED
[RUN] test_sort_stability (Merge Sort)...
  -> PASSED
[RUN] test_union_find_and_trie...
  -> PASSED
[RUN] test_dijkstra_demo_benchmarks (SW to SB shortest vs fastest)...
  SW->SB Shortest distance: 3.2 km (Expected: 3.2 km)
  SW->SB Fastest time: 22.8 min (Expected: 22.8 min)
  -> PASSED
[RUN] test_dynamic_updates_undo_and_bfs...
  After blocking e10: Rerouted! Changed roads: +2 / -1 | Distance: +0.1 km | Time: +3.8 min.
  After Undo: Undo applied: Unblock road e10 | Rerouted! Changed roads: +1 / -2 | Distance: -0.1 km | Time: -3.8 min.
  Components after isolating RZ: 2 (Expected: 2)
  -> PASSED
[RUN] test_tour_planner (must-visit, conflict, budget)...
  Conflict detected successfully: Conflict detected: Must-visit place 'Dagadusheth Halwai Ganapati Temple' (DG) has avoided category 'temple'.
  Tour planned with 4 stops. Total time: 169.8 min (79.8 travel + 90 visit)
  -> PASSED
===========================================
   ALL TESTS PASSED WITH 100% SUCCESS!     
===========================================
```

---

## 7. Data Honesty & Limitations

- **Synthetic Corridor Data**: While attraction names, schematic relative positions, and cultural contexts are real, numerical distances, base transit minutes, traffic levels, and crowd ratings are synthetic classroom sample figures.
- **Simplified Graph Corridors**: Real Pune urban navigation consists of tens of thousands of lane segments and one-way alleys; this project abstracts them into 22 primary arterial corridors to highlight data structure algorithmic mechanics cleanly.
- **Heuristics vs. Exact NP-Hard Solutions**: Tour stop ordering and budget knapsack filtering are heuristic greedy approximations (Nearest-Neighbor, Bounded 2-Opt) rather than exhaustive exponential TSP solvers.

---

## 8. Future Work

1. **OSM / GTFS Integration**: Import OpenStreetMap PBF extracts and Pune Mahanagar Parivahan Mahamandal Ltd (PMPML) bus routes for multi-modal transit graph planning.
2. **Time-Dependent Dijkstra (TDD)**: Model hourly diurnal traffic curves $t(e, \tau)$ to account for morning/evening peak office rushes on Karve Road and FC Road.
3. **Contraction Hierarchies / A\***: Implement preprocessing techniques like Contraction Hierarchies (CH) and bidirectional A* with Landmark heuristics (ALT) for sub-millisecond route queries on million-node networks.
