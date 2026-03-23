#include "book.hpp"
#include <iostream>

int main() {
    Book myBook;

    std::cout << "--- 1. BUILDING THE INITIAL MARKET ---\n";
    // Adding Sellers (Asks)
    myBook.addLimitOrder(1, OrderSide::Sell, 100, 5060);
    myBook.addLimitOrder(2, OrderSide::Sell, 500, 5070);

    // Adding Buyers (Bids)
    myBook.addLimitOrder(3, OrderSide::Buy, 100, 5040);
    
    myBook.printOrderBook();

    std::cout << "--- 2. PLACING THE HIDDEN STOP LIMIT ORDER ---\n";
    std::cout << "Trader places a Buy Stop Limit Order (ID 4) for 50 shares.\n";
    std::cout << "Trigger (Stop) Price: 5060. Execution (Limit) Price: 5055.\n";
    std::cout << "(This order is hidden until the market price hits 5060)\n";
    
    // addStopLimitOrder(orderId, buyOrSell, shares, limitPrice, stopPrice)
    myBook.addStopLimitOrder(4, OrderSide::Buy, 50, 5055, 5060);

    std::cout << "\n--- 3. PUSHING THE MARKET TO TRIGGER THE STOP ---\n";
    std::cout << "Aggressive incoming Market BUY (ID 5) for 50 shares...\n";
    std::cout << "This will execute at 5060, moving the market price and waking up our hidden order.\n";
    
    // This eats 50 shares at 5060, making 5060 the last traded price
    myBook.marketOrder(5, OrderSide::Buy, 50);

    std::cout << "\n--- 4. FINAL ORDER BOOK STATE ---\n";
    std::cout << "Observe the Bids: Order 4 should now be resting safely at 5055!\n";
    myBook.printOrderBook();

    return 0;
}