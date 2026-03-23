#include "limit.hpp"
#include <iostream>

// Constructor initializes the price and side, and starts counters at 0
// Constructor initializes head and tail to "Null" (-1)
Limit::Limit(Price _limitPrice, OrderSide _buyOrSell)
    : limitPrice(_limitPrice), size(0), totalVolume(0), buyOrSell(_buyOrSell),
      headOrder(static_cast<poolIndex>(-1)), tailOrder(static_cast<poolIndex>(-1)) {
}

// Destructor
Limit::~Limit() {
    // std::list cleans up its own memory, so we don't need to do anything here manually.
}

// --- Getters ---
Price Limit::getLimitPrice() const { return limitPrice; }
Size Limit::getSize() const { return size; }
Volume Limit::getTotalVolume() const { return totalVolume; }
OrderSide Limit::getBuyOrSell() const { return buyOrSell; }

poolIndex Limit::getHeadOrder() const { return headOrder; }
poolIndex Limit::getTailOrder() const { return tailOrder; }


void Limit::addOrder(poolIndex orderIdx, Quantity shares, ObjectPool<Order>& pool) {
    Order& newOrder = pool.get(orderIdx);
    
    // The new order goes to the back of the line
    newOrder.setNextOrder(static_cast<poolIndex>(-1));
    newOrder.setPrevOrder(tailOrder);

    if (headOrder == static_cast<poolIndex>(-1)) {
        // Scenario A: The limit level is completely empty. We are the first!
        headOrder = orderIdx;
        tailOrder = orderIdx;
    } else {
        // Scenario B: Orders already exist. Wire the old tail to the new order.
        Order& oldTail = pool.get(tailOrder);
        oldTail.setNextOrder(orderIdx);
        tailOrder = orderIdx;
    }
    
    size++; 
    totalVolume += shares; 
}

void Limit::removeOrder(poolIndex orderIdx, Quantity sharesToRemove, ObjectPool<Order>& pool) {
    Order& order = pool.get(orderIdx);
    poolIndex prev = order.getPrevOrder();
    poolIndex next = order.getNextOrder();

    // 1. Repair the "Next" connection
    if (prev != static_cast<poolIndex>(-1)) {
        // Tell the previous guy to point to the next guy
        pool.get(prev).setNextOrder(next);
    } else {
        // We are deleting the head! The next guy is the new head.
        headOrder = next;
    }

    // 2. Repair the "Previous" connection
    if (next != static_cast<poolIndex>(-1)) {
        // Tell the next guy to point back to the previous guy
        pool.get(next).setPrevOrder(prev);
    } else {
        // We are deleting the tail! The previous guy is the new tail.
        tailOrder = prev;
    }

    // 3. Deduct the math
    totalVolume -= sharesToRemove;
    size--;
}

void Limit::fillVolume(Quantity executedShares) {
    if (totalVolume >= executedShares) {
        totalVolume -= executedShares;
    } else {
        totalVolume = 0; // Safety catch
    }
}

// --- Visualisation ---
void Limit::print() const {
    // std::cout << "Price: " << limitPrice 
    //           << " | Orders: " << size 
    //           << " | Total Volume: " << totalVolume << "\n";
}