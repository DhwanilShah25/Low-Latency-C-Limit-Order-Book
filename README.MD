# High-Performance Limit Order Book (LOB) Matching Engine

A deterministic, ultra-low-latency Limit Order Book (LOB) matching engine written in C++17. Designed specifically for High-Frequency Trading (HFT) environments, this engine prioritizes hardware sympathy, cache locality, and strict zero-allocation on the critical execution path.

## 🚀 Core Engineering Philosophy

1. **Strict Zero-Allocation on the Hot Path:** No calls to the OS kernel (`new`/`delete`) during active trading to avoid page faults and system interruptions.
2. **Hardware Sympathy:** Relentless optimization for CPU L1/L2 cache locality and structural data packing.
3. **Deterministic Latency:** Eliminating mutex contention and complex branching logic to ensure perfectly flat execution times.

---

## 🧠 Architecture Highlights

### 1. Lock-Free Ingress (SPSC Ring Buffer)
The system decouples the ingestion layer from the core matching engine using a **Single-Producer Single-Consumer (SPSC) Ring Buffer**.
- Utilizes `std::atomic` with strict `memory_order_acquire` and `memory_order_release` semantics.
- Bitwise modulo operations guarantee bounded capacities while completely eliminating `std::mutex` contention and OS scheduler yielding.

### 2. Zero-Allocation Hot Path (Object Pool & Intrusive Lists)
Standard dynamic memory allocation induces OS kernel interruptions and heap fragmentation.
- **Object Pool:** Pre-allocates a 10-million-slot contiguous memory pool at startup utilizing a `union`-based free-list array.
- **Intrusive Linked Lists:** Orders are linked at specific price levels using internal integer indices (`nextOrder`, `prevOrder`) rather than heap-allocated pointers, entirely bypassing the OS memory manager during runtime.

### 3. $\mathcal{O}(1)$ Top-of-Book Teleportation (Hardware Bitmasks)
Traditional Red-Black trees (`std::map`) cause pointer-chasing and $\mathcal{O}(\log N)$ latency degradation.
- **FlatMap Structure:** Price levels are mapped to a flat, cache-aligned array.
- **Intrinsic Bitmasking:** Utilizes 64-bit integer masks and compiler intrinsics (`__builtin_ctzll` / `__builtin_clzll`) to calculate leading/trailing zeros. This allows the engine to jump over empty price levels in a single clock cycle, achieving true $\mathcal{O}(1)$ limit discovery.

### 4. Compact Cache-Aligned Structs
Order structures are tightly packed (avoiding heavy metadata strings). This data-oriented design ensures multiple `Order` structs fit seamlessly into a single 64-byte L1 CPU cache line, preventing the CPU from constantly fetching from main memory.

---

## 📁 Project Structure

The codebase is strictly modular, separating the core business logic from the testing and benchmarking suites.

```text
lob-engine/
│
├── CMakeLists.txt              # Standardized build configuration
├── Makefile                    # One-click optimized hardware build targets (-O3, -march=native)
│
├── OrderBook/                  # 🧠 CORE ENGINE
│   ├── types.hpp               # Global type aliases and Enums
│   ├── book.hpp / .cpp         # The core Matching Engine logic
│   ├── limit.hpp / .cpp        # Price level intrusive list managers
│   ├── order.hpp / .cpp        # Cache-aligned 32-byte Order struct
│   ├── pool.hpp                # Union-based zero-allocation ObjectPool
│   ├── flatmap.hpp             # O(1) Bitmask array routing
│   └── ringBuffer.hpp          # Atomic SPSC lock-free queue
│
├── Benchmarking/               # ⚡ STRESS TESTING
│   └── benchmark.cpp           # Google Benchmark suite (Market Sweeps, Ping-Pong fills)
│
├── main.cpp                    # 📖 DEMONSTRATOR: Step-by-step visual terminal output
└── simulator.cpp               # 🌪️ SIMULATOR: 1-Million random transaction chaotic market test