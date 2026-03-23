#include "order.hpp"
#include <iostream>

// Constructor
Order::Order(ID _idNumber, OrderType _type, OrderSide _buyOrSell, Quantity _shares, Price _limitPrice, Price _stopPrice)
    : idNumber(_idNumber), type(_type), buyOrSell(_buyOrSell), shares(_shares), limitPrice(_limitPrice), stopPrice(_stopPrice),
    nextOrder(static_cast<poolIndex>(-1)), prevOrder(static_cast<poolIndex>(-1)) {
}

// --- Getters ---
Quantity Order::getShares() const { return shares; }
ID Order::getOrderId() const { return idNumber; }
OrderSide Order::getBuyOrSell() const { return buyOrSell; }
Price Order::getLimitPrice() const { return limitPrice; } 
OrderType Order::getType() const { return type; }       
Price Order::getStopPrice() const { return stopPrice; }

// --- Modifiers ---
void Order::fill(Quantity executedShares) {
    if (executedShares <= shares) {
        shares -= executedShares;
    } else {
        // std::cerr << "Error: Attempted to over-fill order ID " << idNumber << "\n";
    }
}

// --- INTRUSIVE LIST GETTERS/SETTERS ---
poolIndex Order::getNextOrder() const { return nextOrder; }
poolIndex Order::getPrevOrder() const { return prevOrder; }
void Order::setNextOrder(poolIndex next) { nextOrder = next; }
void Order::setPrevOrder(poolIndex prev) { prevOrder = prev; }

void Order::setShares(Quantity newShares) {
    shares = newShares;
}

// --- Visualisation ---
void Order::print() const {
    // std::cout << "[Order ID: " << idNumber 
    //           << " | Type: " << static_cast<int>(type) // Cast enum to int for quick printing
    //           << " | Side: " << (buyOrSell == OrderSide::Buy ? "BUY" : "SELL") 
    //           << " | Shares: " << shares 
    //           << " | Limit Price: " << limitPrice 
    //           << " | Stop Price: " << stopPrice << "]\n";
}