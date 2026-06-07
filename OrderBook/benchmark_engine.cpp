#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <random>
#include "book.hpp" // Links directly to your matching engine

// Helper to print nice headers
void printHeader(const std::string& title) {
    std::cout << "\n==================================================\n";
    std::cout << "  " << title << "\n";
    std::cout << "==================================================\n";
}

// 1. JITTER & TAIL-LATENCY TEST
void runTailLatencyTest(Book& book, int order_count) {
    printHeader("1. Jitter & Tail-Latency Profile (1M Orders)");
    book.reset(); // Clear engine state safely using your reset() function

    std::vector<uint64_t> latencies;
    latencies.reserve(order_count);

    // Alternate buys and sells to force resting book placement without extensive matching yet
    for (int i = 1; i <= order_count; ++i) {
        ID id = i;
        OrderSide side = (i % 2 == 0) ? OrderSide::Buy : OrderSide::Sell;
        Price price = (side == OrderSide::Buy) ? (10000 - (i % 100)) : (10100 + (i % 100));
        Quantity qty = 100;

        auto start = std::chrono::high_resolution_clock::now();
        book.addLimitOrder(id, side, qty, price);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        latencies.push_back(duration);
    }

    // Sort to extract accurate percentiles
    std::sort(latencies.begin(), latencies.end());

    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    double avg = sum / latencies.size();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Mean Latency:    " << avg << " ns\n";
    std::cout << "Min Latency:     " << latencies[0] << " ns\n";
    std::cout << "p50 (Median):    " << latencies[order_count * 0.50] << " ns\n";
    std::cout << "p90:             " << latencies[order_count * 0.90] << " ns\n";
    std::cout << "p99:             " << latencies[order_count * 0.99] << " ns\n";
    std::cout << "p99.99 (Tail):   " << latencies[order_count * 0.9999] << " ns\n";
    std::cout << "Max Latency:     " << latencies.back() << " ns\n";
}

// 2. BOOK DEPTH & SCALE DEGRADATION TEST
void runScaleDegradationTest(Book& book) {
    printHeader("2. Book Depth & Scale Degradation Test");
    book.reset();

    // Step A: Load the book up with 100,000 resting passive orders
    int base_depth = 100000;
    for (int i = 1; i <= base_depth; ++i) {
        // Distribute bids widely across lower price scales
        book.addLimitOrder(i, OrderSide::Buy, 10, 5000 - (i % 1000));
    }

    // Step B: Benchmark order 100,001 to see if FlatMap scales at O(1)
    auto start = std::chrono::high_resolution_clock::now();
    book.addLimitOrder(100001, OrderSide::Buy, 10, 5001);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    std::cout << "Book Depth size before test: " << base_depth << " resting orders\n";
    std::cout << "Latency to insert order #100,001: " << duration << " ns\n";
    std::cout << "(If this remains near your Mean, your FlatMap lookup is scaling perfectly!)\n";
}

// 3. PASSIVE VS. AGGRESSIVE CASCADE TEST
void runPassiveVsAggressiveTest(Book& book) {
    printHeader("3. Passive vs. Aggressive Cascade Test");
    book.reset();

    // Build a deep layer of matching liquidity on the Ask side across 50 distinct price steps
    ID id_counter = 1;
    for (Price p = 10000; p < 10050; ++p) {
        for (int depth = 0; depth < 10; ++depth) {
            book.addLimitOrder(id_counter++, OrderSide::Sell, 100, p);
        }
    }

    // Scenario A: Insert a Passive Buy order that rests safely at a lower price line
    auto start_passive = std::chrono::high_resolution_clock::now();
    book.addLimitOrder(id_counter++, OrderSide::Buy, 100, 9900);
    auto end_passive = std::chrono::high_resolution_clock::now();
    auto latency_passive = std::chrono::duration_cast<std::chrono::nanoseconds>(end_passive - start_passive).count();

    // Scenario B: Insert an Aggressive Crossing Buy order that wipes out the entire Ask profile
    // Total ask depth constructed = 50 prices * 10 orders * 100 shares = 50,000 shares total
    auto start_aggressive = std::chrono::high_resolution_clock::now();
    book.addLimitOrder(id_counter++, OrderSide::Buy, 50000, 10100); 
    auto end_aggressive = std::chrono::high_resolution_clock::now();
    auto latency_aggressive = std::chrono::duration_cast<std::chrono::nanoseconds>(end_aggressive - start_aggressive).count();

    std::cout << "Passive Order Placement:       " << latency_passive << " ns\n";
    std::cout << "Aggressive Cascade (500 fills): " << latency_aggressive << " ns\n";
    std::cout << "(Verifies speed of Intrusive Pointer unlink loops and ObjectPool deallocations)\n";
}

// 4. THROUGHPUT SATURATION
void runThroughputTest(Book& book, int test_volume) {
    printHeader("4. Throughput Saturation (Max Capacity)");
    book.reset();

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 1; i <= test_volume; ++i) {
        // Alternate crossing transactions to constantly match and cycle through memory loops
        OrderSide side = (i % 2 == 0) ? OrderSide::Buy : OrderSide::Sell;
        book.addLimitOrder(i, side, 100, 10000);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();
    double million_ops_per_sec = (test_volume / elapsed_seconds) / 1000000.0;

    std::cout << "Processed " << test_volume << " crossing orders in: " << elapsed_seconds << " seconds\n";
    std::cout << "Maximum System Throughput Capacity: " << million_ops_per_sec << " Million Messages/sec\n";
}

// 5. WORST-CASE SCENARIO (L1/L2 CACHE MISS TEST)
void runWorstCaseLatencyTest(Book& book, int order_count) {
    printHeader("5. Worst-Case Cache Miss Profile (1M Random Prices)");
    book.reset();

    // 1. Pre-generate 1,000,000 completely random prices
    std::vector<Price> bid_prices(order_count);
    std::vector<Price> ask_prices(order_count);
    
    std::mt19937 rng(1337); 
    // FIX: Strictly segregate Bids and Asks so they NEVER match!
    std::uniform_int_distribution<Price> bid_dist(1, 49000); 
    std::uniform_int_distribution<Price> ask_dist(51000, 100000); 

    for (int i = 0; i < order_count; ++i) {
        bid_prices[i] = bid_dist(rng);
        ask_prices[i] = ask_dist(rng);
    }

    // 2. Batch Timer Setup
    auto start = std::chrono::high_resolution_clock::now();

    // 3. The Choke Loop
    for (int i = 0; i < order_count; ++i) {
        ID id = i + 1;
        OrderSide side = (i % 2 == 0) ? OrderSide::Buy : OrderSide::Sell;
        Price price = (side == OrderSide::Buy) ? bid_prices[i] : ask_prices[i];
        
        book.addLimitOrder(id, side, 100, price);
    }

    auto end = std::chrono::high_resolution_clock::now();

    // 4. Calculate True Average per Order
    double total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double true_average_ns = total_ns / order_count;

    std::cout << "Processed " << order_count << " randomly scattered orders.\n";
    std::cout << "True Average Latency per Insertion: " << true_average_ns << " ns\n";
    std::cout << "(Notice this is higher than your happy-path Mean due to L3 Cache Misses!)\n";
}

int main() {
    Book engine;

    std::cout << "STARTING LIMIT ORDER BOOK PERFORMANCE HARNESS\n";
    std::cout << "==================================================\n";

    // Run tests
    runTailLatencyTest(engine, 1000000);   // 1 Million orders for statistical distribution
    runScaleDegradationTest(engine);       // Testing scaling properties at deep book sizes
    runPassiveVsAggressiveTest(engine);   // Testing execution loop vs passive insertion overhead
    runThroughputTest(engine, 5000000);    // Stress testing max throughput capacity with 5M orders
    runWorstCaseLatencyTest(engine, 1000000);
    return 0;
}