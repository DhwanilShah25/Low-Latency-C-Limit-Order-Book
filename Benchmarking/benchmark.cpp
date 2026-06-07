#include <benchmark/benchmark.h>
#include "../OrderBook/book.hpp" // CMake knows to look in OrderBook/
#include <vector>
#include <algorithm>
#include <random>

// ─────────────────────────────────────────────
// Statistics Helpers (Added P99.9 for HFT safety)
// ─────────────────────────────────────────────
auto p99_stat = [](const std::vector<double>& v) -> double {
    if (v.empty()) return 0;
    std::vector<double> copy = v;
    std::sort(copy.begin(), copy.end());
    return copy[static_cast<size_t>(0.99 * static_cast<double>(copy.size() - 1))];
};

auto p999_stat = [](const std::vector<double>& v) -> double {
    if (v.empty()) return 0;
    std::vector<double> copy = v;
    std::sort(copy.begin(), copy.end());
    return copy[static_cast<size_t>(0.999 * static_cast<double>(copy.size() - 1))];
};

auto median_stat = [](const std::vector<double>& v) -> double {
    if (v.empty()) return 0;
    std::vector<double> copy = v;
    std::sort(copy.begin(), copy.end());
    return copy[copy.size() / 2];
};

// ─────────────────────────────────────────────
// 1. O(1) Scaling Test: addLimitOrder
// ─────────────────────────────────────────────
// Tests scaling from 8 orders up to 131,072 orders. 
// If your FlatMap & DirectMap are truly O(1), the ns/order will remain perfectly flat.
static void BM_AddLimitOrders_Scaling(benchmark::State& state) {
    int numOrders = state.range(0);

    // Construct ONCE outside the loop — pool is warm, caches are stable
    // Book book;

    for (auto _ : state) {
        state.PauseTiming();
        Book book;
        // Only reset the book's internal state, not reconstruct the pool
        // book.reset();  // you'll need to add this method
        state.ResumeTiming();

        for (int i = 0; i < numOrders; ++i) {
            OrderSide side = (i % 2 == 0) ? OrderSide::Buy : OrderSide::Sell;
            Price price = 10000 + (i % 1000) - 25;
            Quantity shares = 100 * ((i % 5) + 1);
            book.addLimitOrder(i, side, shares, price);
        }
        benchmark::DoNotOptimize(book);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * numOrders);
}

BENCHMARK(BM_AddLimitOrders_Scaling)
    ->RangeMultiplier(2)->Range(2048, 2048 << 6) 
    ->Unit(benchmark::kNanosecond)
    ->ComputeStatistics("p99", p99_stat)
    ->ComputeStatistics("p99.9", p999_stat)
    ->ComputeStatistics("median", median_stat)
    ->DisplayAggregatesOnly(true);

// ─────────────────────────────────────────────
// 2. Top-of-Book Ping Pong (Immediate Fills)
// ─────────────────────────────────────────────
// Measures the absolute fastest matching path (Aggressive orders crossing a tight spread)
static void BM_PingPong_Fills(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        Book book;
        // Provide massive resting liquidity at a tight spread ($100.00 x $100.01)
        book.addLimitOrder(1, OrderSide::Buy, 1000000, 10000); // BID
        book.addLimitOrder(2, OrderSide::Sell, 1000000, 10001); // ASK
        uint64_t id = 3;
        state.ResumeTiming();

        // 1. Aggressive Buy (Hits the Ask instantly)
        book.addLimitOrder(id++, OrderSide::Buy, 10, 10001);
        // 2. Aggressive Sell (Hits the Bid instantly)
        book.addLimitOrder(id++, OrderSide::Sell, 10, 10000);

        benchmark::ClobberMemory();
    }
    // Processed 2 executions per iteration
    state.SetItemsProcessed(state.iterations() * 2);
}
BENCHMARK(BM_PingPong_Fills)->Unit(benchmark::kNanosecond)->ComputeStatistics("p99.9", p999_stat)->DisplayAggregatesOnly(true);

// ─────────────────────────────────────────────
// 3. Market Sweep (Walking the Book / Destructive Match)
// ─────────────────────────────────────────────
// Forces the engine to eat through 50 price levels. Tests bitmask scan_up/scan_down speed.
static void BM_MarketSweep(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        Book book;
        uint64_t id = 1;
        // Pre-seed the book with 50 different price levels of Asks (thin liquidity)
        for (int i = 0; i < 50; i++) {
            book.addLimitOrder(id++, OrderSide::Sell, 10, 10000 + i);
        }
        state.ResumeTiming();

        // Send a massive Market Buy order that sweeps all 50 levels
        book.marketOrder(id++, OrderSide::Buy, 500);

        benchmark::ClobberMemory();
    }
    // We process 50 individual fills per iteration
    state.SetItemsProcessed(state.iterations() * 50); 
}
BENCHMARK(BM_MarketSweep)->Unit(benchmark::kNanosecond)->ComputeStatistics("p99.9", p999_stat)->DisplayAggregatesOnly(true);

// ─────────────────────────────────────────────
// 4. Market Maker Workload (Heavy Cancel Ratio)
// ─────────────────────────────────────────────
// Simulates realistic trading: Rapid adds, followed by massive cancellations, then a few fills.
static void BM_MarketMakerChurn(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        Book book;
        uint64_t id = 1;
        
        // Pre-warm the book with some liquidity
        for(int i = 0; i < 1000; i++) {
            book.addLimitOrder(id++, OrderSide::Buy, 100, 9900 + (i%50));
            book.addLimitOrder(id++, OrderSide::Sell, 100, 10100 - (i%50));
        }
        state.ResumeTiming();

        uint64_t cycleStartId = id;

        // 1. Market Maker adds 20 new limit orders rapidly
        for (int i = 0; i < 20; i++) {
            book.addLimitOrder(id++, OrderSide::Buy, 100, 10000 - i);
        }
        
        // 2. Market Maker realizes the market is moving and cancels 19 of them instantly
        for (int i = 0; i < 19; i++) {
            book.cancelLimitOrder(cycleStartId + i); 
        }
        
        // 3. Fire a market order to execute against the 1 remaining order
        book.marketOrder(id++, OrderSide::Sell, 100);

        benchmark::ClobberMemory();
    }
    // Processed 40 operations (20 adds, 19 cancels, 1 market execution)
    state.SetItemsProcessed(state.iterations() * 40);
}

BENCHMARK(BM_MarketMakerChurn)
    ->Unit(benchmark::kNanosecond)
    ->ComputeStatistics("p99", p99_stat)
    ->ComputeStatistics("p99.9", p999_stat)
    ->ComputeStatistics("median", median_stat)
    ->DisplayAggregatesOnly(true);


// ─────────────────────────────────────────────
// 5. Worst-Case Cache Miss Profile (Random Memory Jumps)
// ─────────────────────────────────────────────
// Forces the CPU to jump wildly across the FlatMap's 8MB memory block, 
// causing intentional L1/L2 cache misses to find the true worst-case latency.
static void BM_WorstCaseCacheMiss(benchmark::State& state) {
    const int numOrders = state.range(0);

    // 1. Pre-generate data OUTSIDE the timed loop!
    std::vector<Price> bid_prices(numOrders);
    std::vector<Price> ask_prices(numOrders);
    
    std::mt19937 rng(1337); 
    // Strictly segregate Bids and Asks so they NEVER match (we only want to test insertion jumping)
    std::uniform_int_distribution<Price> bid_dist(1, 49000); 
    std::uniform_int_distribution<Price> ask_dist(51000, 100000); 

    for (int i = 0; i < numOrders; ++i) {
        bid_prices[i] = bid_dist(rng);
        ask_prices[i] = ask_dist(rng);
    }

    // 2. The Timed Benchmark Loop
    for (auto _ : state) {
        state.PauseTiming();
        Book book; // Re-instantiate a clean book
        state.ResumeTiming();

        // 3. Blast the pre-computed random orders
        for (int i = 0; i < numOrders; ++i) {
            OrderSide side = (i % 2 == 0) ? OrderSide::Buy : OrderSide::Sell;
            Price price = (side == OrderSide::Buy) ? bid_prices[i] : ask_prices[i];
            
            book.addLimitOrder(i + 1, side, 100, price);
        }
        
        // Force the compiler to actually perform the memory writes
        benchmark::ClobberMemory();
    }
    
    // Tell Google Benchmark exactly how many operations we performed 
    // so it can accurately calculate the nanoseconds-per-order.
    state.SetItemsProcessed(state.iterations() * numOrders);
}

// We will test this with 10,000 orders, 100,000 orders, and 1,000,000 orders.
BENCHMARK(BM_WorstCaseCacheMiss)
    ->RangeMultiplier(10)->Range(10000, 1000000) 
    ->Unit(benchmark::kNanosecond)
    ->ComputeStatistics("p99", p99_stat)
    ->ComputeStatistics("median", median_stat)
    ->DisplayAggregatesOnly(true);

BENCHMARK_MAIN();