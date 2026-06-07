#include "../OrderBook/book.hpp"
#include <iostream>
#include <vector>
#include <random>
#include <chrono>

// Define the types of actions our simulator will take
enum class ActionType { ADD, CANCEL, MODIFY, MARKET };

struct MarketAction {
    ActionType type;
    ID id;
    OrderSide side;
    Quantity qty;
    Price price;
};

int main() {
    Book myBook;
    const int INITIAL_ORDERS = 10000;
    const int NUM_TRANSACTIONS = 1000000;
    
    std::cout << "--- PHASE 1: Setting up Market Generator ---\n";
    std::mt19937 rng(42); // Fixed seed for reproducible benchmarks
    std::uniform_int_distribution<int> priceDist(4900, 5100);
    std::uniform_int_distribution<int> qtyDist(10, 500);
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::uniform_int_distribution<int> actionDist(1, 100);

    ID currentOrderId = 1;
    std::vector<ID> activeIds; // Keep track of IDs so we can cancel/modify them
    activeIds.reserve(INITIAL_ORDERS + NUM_TRANSACTIONS);

    // 1. Seed the initial book
    for (int i = 0; i < INITIAL_ORDERS; ++i) {
        OrderSide side = (sideDist(rng) == 0) ? OrderSide::Buy : OrderSide::Sell;
        myBook.addLimitOrder(currentOrderId, side, qtyDist(rng), priceDist(rng));
        activeIds.push_back(currentOrderId);
        currentOrderId++;
    }

    std::cout << "--- PHASE 2: Pre-generating 1,000,000 chaotic market actions ---\n";
    std::vector<MarketAction> actions;
    actions.reserve(NUM_TRANSACTIONS);

    for (int i = 0; i < NUM_TRANSACTIONS; ++i) {
        int r = actionDist(rng);
        MarketAction action;
        
        // Distribution: 45% Add, 35% Cancel, 10% Modify, 10% Market Order
        if (r <= 45) {
            action.type = ActionType::ADD;
            action.id = currentOrderId++;
            action.side = (sideDist(rng) == 0) ? OrderSide::Buy : OrderSide::Sell;
            action.qty = qtyDist(rng);
            action.price = priceDist(rng);
            activeIds.push_back(action.id);
        } 
        else if (r <= 80) {
            action.type = ActionType::CANCEL;
            // Pick a random recently added ID to cancel
            action.id = activeIds[rng() % activeIds.size()]; 
        } 
        else if (r <= 90) {
            action.type = ActionType::MODIFY;
            action.id = activeIds[rng() % activeIds.size()];
            action.qty = qtyDist(rng);
            action.price = priceDist(rng);
        } 
        else {
            action.type = ActionType::MARKET;
            action.id = currentOrderId++;
            action.side = (sideDist(rng) == 0) ? OrderSide::Buy : OrderSide::Sell;
            action.qty = qtyDist(rng); // Usually smaller than limit orders to prevent completely emptying the book
        }
        actions.push_back(action);
    }

    std::cout << "--- PHASE 3: FIRING THE SPRINT ---\n";
    std::cout << "Executing 1,000,000 transactions...\n";

    // START THE CLOCK
    auto start = std::chrono::high_resolution_clock::now();

    for (const auto& action : actions) {
        switch (action.type) {
            case ActionType::ADD:
                myBook.addLimitOrder(action.id, action.side, action.qty, action.price);
                break;
            case ActionType::CANCEL:
                myBook.cancelLimitOrder(action.id);
                break;
            case ActionType::MODIFY:
                myBook.modifyLimitOrder(action.id, action.qty, action.price);
                break;
            case ActionType::MARKET:
                myBook.marketOrder(action.id, action.side, action.qty);
                break;
        }
    }

    // STOP THE CLOCK
    auto end = std::chrono::high_resolution_clock::now();
    
    // Calculate results
    std::chrono::duration<double> diff = end - start;
    double seconds = diff.count();
    double tps = NUM_TRANSACTIONS / seconds;

    std::cout << "\n========== BENCHMARK RESULTS ==========\n";
    std::cout << "Total Transactions : " << NUM_TRANSACTIONS << "\n";
    std::cout << "Total Time         : " << seconds << " seconds\n";
    std::cout << "Average Latency    : " << (seconds * 1'000'000'000) / NUM_TRANSACTIONS << " ns per order\n";
    std::cout << "Throughput (TPS)   : " << static_cast<long long>(tps) << " transactions/sec\n";
    std::cout << "=======================================\n";

    return 0;
}