#pragma once

#ifndef ORDER_HPP
#define ORDER_HPP

#include"types.hpp"

// Standard Libraries
#include <cstdint> // For strict width integer types
#include<iostream>

class Order {
private:
    ID idNumber;
    OrderType type;
    OrderSide buyOrSell;
    Quantity shares;
    Price limitPrice;
    Price stopPrice;

    // --- INTRUSIVE LIST POINTERS ---
    // These replace std::list! They point to the poolIndex of the adjacent orders.
    poolIndex nextOrder;
    poolIndex prevOrder;
    
public:
    Order(ID _idNumber, OrderType _type, OrderSide _buyOrSell, Quantity _shares, Price _limitPrice, Price _stopPrice = 0);
    ~Order() = default;

    // Getters
    Quantity getShares() const;
    ID getOrderId() const;
    OrderSide getBuyOrSell() const;
    Price getLimitPrice() const;
    OrderType getType() const;
    Price getStopPrice() const;

    // --- INTRUSIVE LIST GETTERS/SETTERS ---
    poolIndex getNextOrder() const;
    poolIndex getPrevOrder() const;
    void setNextOrder(poolIndex next);
    void setPrevOrder(poolIndex prev);

    // Modifiers
    void fill(Quantity executedShares);
    void setShares(Quantity newShares);

    // Visualisation
    void print() const;

};

#endif