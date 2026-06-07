# High-Performance Limit Order Book (LOB) Matching Engine

A deterministic, ultra-low-latency Limit Order Book (LOB) matching engine written in C++17. Designed specifically for High-Frequency Trading (HFT) environments, this engine prioritizes hardware sympathy, CPU cache locality, and strict zero-allocation on the critical execution path.

## Core Engineering Philosophy

1. **Strict Zero-Allocation on the Hot Path:** No calls to the OS kernel (`new`/`delete`) during active trading to avoid page faults and system interruptions.
2. **Hardware Sympathy:** Relentless optimization for CPU L1/L2 cache locality and structural data packing.
3. **Deterministic Latency:** Eliminating OS mutex locks, pointer-chasing, and complex branching logic to ensure perfectly flat execution times regardless of market volatility.

---

## Architecture Highlights

### 1. Lock-Free Inter-Thread Communication (SPSC Ring Buffer)
The system safely transfers data between the ingestion thread and the matching engine thread without using slow OS-level locks (`std::mutex`).
- **Atomic Memory Barriers:** Utilizes `std::atomic` with strict `memory_order_acquire` and `memory_order_release` semantics to safely hand off data across CPU cores.
- **Wait-Free Execution:** By relying on bitwise modulo operations on a power-of-2 capacity array, the buffer prevents the OS thread scheduler from ever suspending the core engine.

### 2. Zero-Allocation Hot Path (Object Pool & Intrusive Lists)
Standard dynamic memory allocation induces OS kernel interruptions and heap fragmentation.
- **Object Pool:** Pre-allocates a 10-million-slot contiguous memory pool at startup utilizing a `union`-based free-list array.
- **Intrusive Linked Lists:** Orders are linked at specific price levels using internal integer indices (`nextOrder`, `prevOrder`) rather than heap-allocated pointers. This entirely bypasses the OS memory manager during runtime and ensures tight memory packing.

### 3. $\mathcal{O}(1)$ Top-of-Book Teleportation (Hardware Bitmasks)
Traditional Red-Black trees (`std::map`) cause pointer-chasing and $\mathcal{O}(\log N)$ latency degradation as the order book deepens.
- **FlatMap Structure:** Price levels are mapped directly to a flat, cache-aligned array.
- **Intrinsic Bitmasking:** Utilizes 64-bit integer masks and compiler intrinsics (`__builtin_ctzll` / `__builtin_clzll`) to calculate leading/trailing zeros. This allows the engine to jump over empty price levels in a single clock cycle, achieving true mathematical $\mathcal{O}(1)$ limit discovery.

### 4. Compact Cache-Aligned Structs
Order structures are tightly packed using strict-width integers (`uint32_t`, `uint64_t`). This data-oriented design ensures multiple `Order` structs fit seamlessly into a single 64-byte L1 CPU cache line, preventing the CPU from constantly fetching from main memory during massive trade cascades.

---

## Performance & Benchmarks (Google Benchmark)

The engine's performance is verified using the industry-standard `Google Benchmark` library, compiled with `-O3` optimizations. 

### 1. The $\mathcal{O}(1)$ Scaling Proof
As the order book grows, standard engines slow down. Because this engine utilizes a contiguous `ObjectPool` and a flat array, heavier batching actually *warms up* the CPU's L1 cache and branch predictor, resulting in **increased throughput at scale**.

| Orders Processed | CPU Time per Batch | Throughput (Items / Second) |
| :--- | :--- | :--- |
| 2,048 | ~768,779 ns | 2.66 Million / sec |
| 16,384 | ~1,421,068 ns | 11.52 Million / sec |
| **131,072** | **~6,182,134 ns** | **21.20 Million / sec** |

### 2. Worst-Case Cache Miss Resilience
To test the engine's architectural limit, `1,000,000` orders were blasted at completely randomized, heavily scattered prices across the 8MB `FlatMap` array to force L2/L3 cache misses.
- **Result:** The engine successfully absorbed the entropy, maintaining an average insertion latency of **~58 nanoseconds per order** (17.2 Million items/sec). 

### 3. Trade Execution Speed
An Aggressive Cascade test executing massive market sweeps and destroying hundreds of resting orders confirmed that individual trade matches, intrusive pointer rewiring, and memory deallocation take roughly **~30 nanoseconds** per execution.

---

## Project Structure

The codebase is strictly modular, separating the core business logic from the benchmarking suites.

```text
lob-engine/
│
├── CMakeLists.txt              # Standardized build configuration
│
├── src/                        # CORE ENGINE
│   ├── types.hpp               # Global type aliases and Enums
│   ├── book.hpp / .cpp         # The core Matching Engine logic
│   ├── limit.hpp / .cpp        # Price level intrusive list managers
│   ├── order.hpp / .cpp        # Cache-aligned 32-byte Order struct
│   ├── pool.hpp                # Union-based zero-allocation ObjectPool
│   └── flatmap.hpp             # O(1) Bitmask array routing
│
├── benchmarks/                 # STRESS TESTING
│   ├── benchmark.cpp           # Google Benchmark suite (Scaling, Cache, Sweeps)
│   └── simulator.cpp           # 1-Million random transaction chaotic market test
│
└── network/                    # GATEWAY (WIP Prototype)
    ├── ringBuffer.hpp          # Atomic SPSC lock-free queue
    └── main.cpp                # Simple TCP Ingress / UDP Egress Broadcaster