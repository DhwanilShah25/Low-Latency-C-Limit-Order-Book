# Architecture & Design Decisions

## Core Engineering Philosophy

This Limit Order Book (LOB) is designed for deterministic, low-latency environments. The architecture prioritizes three absolute mandates:

1. **Strict Zero-Allocation on the Hot Path:** No calls to the OS kernel (`new`/`delete`) during active trading to avoid page faults and system interruptions.
2. **Hardware Sympathy:** Relentless optimization for CPU L1/L2 cache locality and data packing.
3. **Deterministic Latency:** Eliminating mutex contention and complex branching logic to ensure flat execution times.

---

## 1. The Order Queue (Time Priority)

**Goal:** Maintain strict First-In-First-Out (FIFO) execution for orders at the same price level.

* **Attempted:** `std::list<Order>`
* **Rejected:** The standard library doubly-linked list allocates a new heap node for every single order. This forces the engine to pause and ask the OS for memory, consuming massive CPU time in early profiles. It also scatters orders randomly across RAM, causing severe L1 cache misses.
* **Adopted:** **Custom Contiguous `ObjectPool` + Intrusive Linked List**
* **Rationale:** The engine pre-allocates a capacity-defined `std::vector` (the union-based memory pool) at startup. When an order arrives, it uses a pre-allocated slot. The `Order` struct itself contains `nextOrder` and `prevOrder` integer indices, forming an intrusive list that entirely bypasses dynamic heap allocations on the hot path.

## 2. The Price Levels (Price Priority)

**Goal:** Route orders to specific price levels and find the next best Bid/Ask instantly.

* **Attempted:** `std::map<Price, Limit>` (Red-Black Trees)
* **Rejected:** Binary trees require pointer-chasing. Finding the next best price requires traversal through scattered memory nodes, ruining cache locality.
* **Adopted:** **Flat Array (`FlatMap`) with Hardware Bitmasks**
* **Rationale:** Price levels are stored in a massive, flat `std::vector`. To solve the issue of iterating over empty price slots, the engine uses an array of `uint64_t` bitmasks. Each 64-bit integer acts as a "radar screen" for 64 price levels. Using hardware intrinsics (`__builtin_ctzll` and `__builtin_clzll`), the CPU counts leading/trailing zeros in a single clock cycle, mathematically calculating the exact index of the next active price level and allowing for O(1) top-of-book lookups.

## 3. Order ID Tracking (The Routing Map)

**Goal:** Instantly locate an order's memory pool index when a trader sends a "Cancel" or "Modify" request via their Order ID.

* **Current Architecture:** **Standard Hash Map (`std::unordered_map`)**
* **Rationale:** The engine currently utilizes `std::unordered_map<ID, poolIndex>` to achieve O(1) average-case lookups for routing cancellation and modification requests.
* **Future Optimization (Direct Array Addressing):** While the hash map provides rapid routing, hash collisions and bucket node allocations can introduce minor latency variations. The roadmap includes migrating to a flat `std::vector<poolIndex>` where tightly bounded, sequential Order IDs function directly as the array index, reducing lookup math to a single CPU instruction.

## 4. Data Layout (Memory Footprint)

**Goal:** Prevent large string variables from bloating the cache line and degrading iteration speed.

* **Attempted:** Storing all metadata (Trader Name, Timestamps, message strings) inside the core `Order` struct.
* **Rejected:** A CPU cache line is exactly 64 bytes. Bloated structs mean fewer orders fit into the L1 cache at a time, forcing the CPU to constantly fetch from main memory.
* **Adopted:** **Compact Cache-Aligned Structs**
* **Rationale:** The matching engine enforces a ruthless "Hot Struct" policy. The core `Order` struct contains only the mathematical bare minimum required to execute a trade (Price, Size, Side, IDs, Intrusive Pointers). This compression allows multiple orders to fit into a single cache line. Heavy "Cold Data" is relegated to external applications and never pollutes the core matching loop.

## 5. Thread Communication & Ingress

**Goal:** Pass incoming orders from the Gateway process to the Matching Engine without blocking or data races.

* **Attempted:** `std::mutex` with a standard `std::queue`.
* **Rejected:** Locking a mutex forces the thread to yield to the OS scheduler, introducing massive, unpredictable latency spikes whenever the Gateway and Engine attempt to access the queue simultaneously.
* **Adopted:** **Lock-Free SPSC Ring Buffer**
* **Rationale:** The engine connects the ingestion layer to the core loop using a Single-Producer Single-Consumer (SPSC) queue. It utilizes `std::atomic` variables with explicit memory ordering (`memory_order_acquire` and `memory_order_release`). By enforcing a queue capacity that is a strict power of 2, the buffer utilizes bitwise modulo operations to safely and continuously stream binary structs without ever triggering an OS lock.