#include "book.hpp"
#include<vector>
// --- Constructor ---
Book::Book() {
    // Maps are automatically initialized
}

// --- Internal Helper ---
void Book::deleteFromOrderMap(ID orderId) {
    orderMap.erase(orderId);
}

void Book::reset() {
    // Clear FlatMaps (erase all set bits and optionals)
    for (auto it = bids.begin(); it != bids.end(); ) it = bids.erase(it);
    for (auto it = asks.begin(); it != asks.end(); ) it = asks.erase(it);

    // Clear lookup maps
    orderMap.clear();

    // Reset pool free list without reallocating
    orderPool.reset();
}


// --- Core Matching Engine API ---

void Book::addLimitOrder(ID orderId, OrderSide buyOrSell, Quantity shares, Price limitPrice) {
    // 1. Prevent duplicate IDs
    if (orderMap.find(orderId) != orderMap.end()) {
        std::cerr << "Error: Order ID " << orderId << " already exists.\n";
        return;
    }

    Quantity remainingShares = shares;
    Price lastTradedPrice = 0;
    bool tradeOccurred = false;

    // 2. Route to Bids or Asks and check for Marketable overlaps
    if (buyOrSell == OrderSide::Buy) {
        
        // AGGRESS THE BOOK: Check if the Limit BUY crosses the lowest Ask
        auto limitIt = asks.begin();
        while (limitIt != asks.end() && limitIt->first <= limitPrice && remainingShares > 0) {
            Limit& currentLimit = limitIt->second;
            poolIndex currIdx = currentLimit.getHeadOrder();

            while (currIdx != static_cast<poolIndex>(-1) && remainingShares > 0) {
                Order& restingOrder = orderPool.get(currIdx);
                Quantity restingShares = restingOrder.getShares();
                
                // Save the next pointer BEFORE we potentially destroy this order!
                poolIndex nextIdx = restingOrder.getNextOrder();

                if (restingShares > remainingShares) {
                    restingOrder.fill(remainingShares);
                    currentLimit.fillVolume(remainingShares);
                    lastTradedPrice = currentLimit.getLimitPrice();
                    tradeOccurred = true;
                    remainingShares = 0;
                    break; 
                } else {
                    lastTradedPrice = currentLimit.getLimitPrice();
                    tradeOccurred = true;
                    remainingShares -= restingShares;
                    deleteFromOrderMap(restingOrder.getOrderId());

                    currentLimit.removeOrder(currIdx, restingShares, orderPool);
                    // POOL: The resting order is dead. Destroy it to free the memory!
                    orderPool.destroy(currIdx);
                    // POOL: Tell the Limit level to remove the iterator AND deduct the volume
                    currIdx = nextIdx;
                }
            }

            if (currentLimit.getSize() == 0) {
                limitIt = asks.erase(limitIt);
            } 
            else {
                /* Honestly this means that remaining shares have got zero, then why do we increment here?
                 
                Ans. Just as a custom C++ code, every single path through the loop must either erase the 
                iterator or increment the iterator. So that next time some one tweaks code with say 
                removing (remaining shares>0) condition, the code still does not crash! */

                /* NOTE: It only mahtematically gurantees code doesn't crash, but it does not guarantee 
                that the system works as desired! */

                ++limitIt; 
            }
        }

        // REST IN BOOK: If shares remain after eating liquidity, rest them at the limit price
        if (remainingShares > 0) {
            poolIndex newOrderIndex = orderPool.construct(orderId, OrderType::Limit, buyOrSell, remainingShares, limitPrice);
            // Order newOrder(orderId, OrderType::Limit, buyOrSell, remainingShares, limitPrice);
            auto it = bids.find(limitPrice);
            if (it == bids.end()) {
                it = bids.emplace(limitPrice, Limit(limitPrice, OrderSide::Buy)).first;
            }
            it->second.addOrder(newOrderIndex, remainingShares, orderPool);
            orderMap[orderId] = newOrderIndex;
        }

    } else { // OrderSide::Sell
        
        // AGGRESS THE BOOK: Check if the Limit SELL crosses the highest Bid
        auto limitIt = bids.begin();
        while (limitIt != bids.end() && limitIt->first >= limitPrice && remainingShares > 0) {
            Limit& currentLimit = limitIt->second;
            poolIndex currIdx = currentLimit.getHeadOrder();

            while (currIdx!=static_cast<poolIndex>(-1) && remainingShares > 0) {
                
                Order& restingOrder = orderPool.get(currIdx);
                Quantity restingShares = restingOrder.getShares();

                poolIndex nextIdx = restingOrder.getNextOrder();

                if (restingShares > remainingShares) {
                    restingOrder.fill(remainingShares);
                    currentLimit.fillVolume(remainingShares);
                    lastTradedPrice = currentLimit.getLimitPrice();
                    tradeOccurred = true;
                    remainingShares = 0;
                    break;
                } else {
                    lastTradedPrice = currentLimit.getLimitPrice();
                    tradeOccurred = true;
                    remainingShares -= restingShares;
                    deleteFromOrderMap(restingOrder.getOrderId());
                    // POOL: The resting order is dead. Destroy it to free the memory!
                    currentLimit.removeOrder(currIdx, restingShares, orderPool);
                    orderPool.destroy(currIdx);
                    // POOL: Tell the Limit level to remove the iterator AND deduct the volume
                    currIdx=nextIdx;
                }
            }

            if (currentLimit.getSize() == 0) {
                limitIt = bids.erase(limitIt);
            } else {
                ++limitIt;
            }
        }

        // REST IN BOOK: Rest remaining shares
        if (remainingShares > 0) {
            poolIndex newOrderIndex = orderPool.construct(orderId, OrderType::Limit, buyOrSell, remainingShares, limitPrice);
            // Order newOrder(orderId, OrderType::Limit, buyOrSell, remainingShares, limitPrice);
            auto it = asks.find(limitPrice);
            if (it == asks.end()) {
                it = asks.emplace(limitPrice, Limit(limitPrice, OrderSide::Sell)).first;
            }
            it->second.addOrder(newOrderIndex,remainingShares, orderPool);
            orderMap[orderId] = newOrderIndex;
        }
    }
}

void Book::cancelLimitOrder(ID orderId) {
    // 1. O(1) Lookup: Find the order instantly
    auto mapIt = orderMap.find(orderId);
    if (mapIt == orderMap.end()) {
        // std::cerr << "Error: Cannot cancel. Order ID " << orderId << " not found.\n";
        return;
    }

    // 2. Extract order details via the iterator

    poolIndex deadIdx = mapIt->second;

    auto &order = orderPool.get(deadIdx);
    OrderSide side = order.getBuyOrSell();
    Price price = order.getLimitPrice();
    Quantity shares = order.getShares();

    // 3. Go to the correct map and limit level to remove it
    if (side == OrderSide::Buy) {
        auto limitIt = bids.find(price);
        if (limitIt != bids.end()) {
    
            // Remove order from Limit
            limitIt->second.removeOrder(deadIdx,shares,orderPool);
            
            // Clean up the price level if it's empty
            if (limitIt->second.getSize() == 0) {
                bids.erase(limitIt);
            }
        }
    } else { // OrderSide::Sell
        auto limitIt = asks.find(price);
        if (limitIt != asks.end()) {
            limitIt->second.removeOrder(deadIdx,shares,orderPool);
            if (limitIt->second.getSize() == 0) {
                asks.erase(limitIt);
            }
        }
    }
    // 4. Give the memory back to the pool BEFORE we leave the function!
    orderPool.destroy(deadIdx);
    // 5. Finally, remove it from our O(1) lookup map
    deleteFromOrderMap(orderId);
}


/*
The "Wall Street" Rules of Order Modification

When a trader wants to change an existing limit order, exchanges have very strict rules 
about Time Priority (your place in line):

1. Changing the Price: If you change your price, you are entirely removed from your current queue 
    and placed at the back of the new price level\'s queue. (Loses priority).

2. Increasing the Size: If you keep the same price but want to add more shares, you lose your spot 
    and go to the back of the line. (Loses priority).

3. Decreasing the Size: If you keep the same price and just want to cancel part of your order, 
    the exchange is nice to you. You get to keep your exact spot in line. (Keeps priority).*/

void Book::modifyLimitOrder(ID orderId, Quantity newShares, Price newLimitPrice) {
    // 1. Find the existing order
    auto mapIt = orderMap.find(orderId);
    if (mapIt == orderMap.end()) {
        // std::cerr << "Error: Cannot modify. Order ID " << orderId << " not found.\n";
        return;
    }

    // 2. Extract current details
    poolIndex orderIdx = mapIt->second;
    auto &order = orderPool.get(orderIdx);
    OrderSide side = order.getBuyOrSell();
    Price currentPrice = order.getLimitPrice();
    Quantity currentShares = order.getShares();

    // 3. Check the "Wall Street" Rules

    // Traders will sometimes try to cancel an order by sending a "Modify" request with newShares = 0.
    if (newShares == 0) {
        cancelLimitOrder(orderId);
        return;
    }

    if (newLimitPrice == currentPrice && newShares < currentShares) {
        // RULE 3: Same price, smaller size. Keep our place in line!
        Quantity difference = currentShares - newShares;
        
        // Update the Order's internal share count
        order.setShares(newShares);
        
        // Find the Limit level and deduct the volume difference
        if (side == OrderSide::Buy) {
            bids.at(currentPrice).fillVolume(difference); 
            // Note: We use fillVolume here just to safely subtract from totalVolume 
            // without changing the 'size' (order count), since the order still exists.
        } else {
            asks.at(currentPrice).fillVolume(difference);
        }
        
        // std::cout << "[MODIFY] Order " << orderId << " reduced to " << newShares 
        //           << " shares. Time priority maintained.\n";
                  
    } else {
        // RULES 1 & 2: Price changed OR size increased. We lose our place in line.
        // std::cout << "[MODIFY] Order " << orderId << " changed price or increased size. "
        //           << "Canceling and replacing at the back of the queue.\n";
                  
        cancelLimitOrder(orderId);
        addLimitOrder(orderId, side, newShares, newLimitPrice);
    }
}

void Book::marketOrder(ID orderId, OrderSide buyOrSell, Quantity shares) {
    Quantity remainingShares = shares;

    bool tradeOccurred = false;   

    // std::cout << "\n[EXCHANGE] Processing Market " << (buyOrSell == OrderSide::Buy ? "BUY" : "SELL") 
    //           << " Order " << orderId << " for " << shares << " shares.\n";

    if (buyOrSell == OrderSide::Buy) {
        auto limitIt = asks.begin();
        
        while (limitIt != asks.end() && remainingShares > 0) {
            Limit& currentLimit = limitIt->second;
            poolIndex orderIdx = currentLimit.getHeadOrder();

            while (orderIdx != static_cast<poolIndex>(-1) && remainingShares > 0) {
                Order& restingOrder = orderPool.get(orderIdx);
                Quantity restingShares = restingOrder.getShares();

                poolIndex nextOrder = restingOrder.getNextOrder();
                if (restingShares > remainingShares) {
                    // Scenario A: Resting limit order is bigger than our market order
                    restingOrder.fill(remainingShares);
                    currentLimit.fillVolume(remainingShares);
                    
                    // std::cout << "   -> Filled " << remainingShares << " shares @ " 
                    //           << currentLimit.getLimitPrice() << " (Order " << restingOrder.getOrderId() << " partially filled)\n";
                    
                    tradeOccurred = true;

                    remainingShares = 0;
                    break; 
                } else {
                    // Scenario B: Market order completely eats this resting limit order
                    // std::cout << "   -> Filled " << restingShares << " shares @ " 
                    //           << currentLimit.getLimitPrice() << " (Order " << restingOrder.getOrderId() << " fully filled)\n";
                    
                    tradeOccurred = true;

                    remainingShares -= restingShares;
                    
                    // 1. Remove from O(1) lookup map BEFORE erasing memory
                    deleteFromOrderMap(restingOrder.getOrderId());

                    // 2. Tell the Limit level to rewire pointers and deduct volume/size!
                    currentLimit.removeOrder(orderIdx, restingShares, orderPool);
                    // 3. Remove from orderPool
                    orderPool.destroy(orderIdx);
                    // 4. Limit handles math and memory, and hands us back the next iterator!
                    orderIdx = nextOrder; 
                }
            }

            if (currentLimit.getSize() == 0) {
                limitIt = asks.erase(limitIt);
            } else {
                ++limitIt; 
            }
        }
    } 
    else {
        auto limitIt = bids.begin();
        
        while (limitIt != bids.end() && remainingShares > 0) {
            Limit& currentLimit = limitIt->second;
            poolIndex orderIdx = currentLimit.getHeadOrder();

            
            while (orderIdx != static_cast<poolIndex>(-1) && remainingShares > 0) {
                Order& restingOrder = orderPool.get(orderIdx);
                Quantity restingShares = restingOrder.getShares();
                poolIndex nextOrderIdx = restingOrder.getNextOrder();

                if (restingShares > remainingShares) {
                    restingOrder.fill(remainingShares);
                    currentLimit.fillVolume(remainingShares);
                    
                    // std::cout << "   -> Filled " << remainingShares << " shares @ " 
                    //           << currentLimit.getLimitPrice() << " (Order " << restingOrder.getOrderId() << " partially filled)\n";
                    
                    //Update lastTradedPrice
                    tradeOccurred = true;

                    remainingShares = 0;
                    break;
                } else {
                    // std::cout << "   -> Filled " << restingShares << " shares @ " 
                    //           << currentLimit.getLimitPrice() << " (Order " << restingOrder.getOrderId() << " fully filled)\n";
                    
                    tradeOccurred = true;

                    remainingShares -= restingShares;
                    deleteFromOrderMap(restingOrder.getOrderId());

                    //Tell the Limit level to rewire pointers and deduct volume/size!
                    currentLimit.removeOrder(orderIdx, restingShares, orderPool);

                    // Remove from Pool
                    orderPool.destroy(orderIdx);
                    // Use the new Limit method here too
                    orderIdx = nextOrderIdx;
                }
            }

            if (currentLimit.getSize() == 0) {
                limitIt = bids.erase(limitIt);
            } else {
                ++limitIt;
            }
        }
    }

    if (remainingShares > 0) {
        // std::cout << "[WARNING] Market order " << orderId << " partially filled. " 
        //           << remainingShares << " shares remain unexecuted due to lack of liquidity.\n";
    }
}


// --- Visualisation ---

void Book::printOrderBook() const {
    std::cout << "\n========== ORDER BOOK ==========\n";
    
    std::cout << "--- ASKS (Sellers) ---\n";
    // Asks are lowest to highest naturally. To print highest on top, we use reverse iterators.
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        it->second.print();
    }

    std::cout << "\n--- BIDS (Buyers) ---\n";
    // Bids are mapped with std::greater, so standard iteration prints highest to lowest.
    for (const auto& pair : bids) {
        pair.second.print();
    }
    std::cout << "================================\n\n";
}

void Book::printOrder(ID orderId) const {
    auto it = orderMap.find(orderId);
    if (it != orderMap.end()) {
        poolIndex orderIdx = (it->second);
        const Order& order = orderPool.get(orderIdx);
        order.print();
    } else {
        std::cout << "Order " << orderId << " not found.\n";
    }
}
