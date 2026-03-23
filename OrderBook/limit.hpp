#pragma once

#ifndef LIMIT_HPP
#define LIMIT_HPP

#include"types.hpp"
#include"order.hpp"
#include "pool.hpp"

// Standard Libraries
#include <cstdint> // For strict width integer types
#include<list>

class Limit{
private:
    Price limitPrice;
    Size size;
    Volume totalVolume;
    OrderSide buyOrSell;

    // --- INTRUSIVE LIST TRACKERS ---
    // Replacing std::list! We only need to know where the line starts and ends.
    poolIndex headOrder;
    poolIndex tailOrder;
public:
    Limit(Price _limitPrice, OrderSide _buyOrSell);
    ~Limit();

    Price getLimitPrice() const;
    Size getSize() const;
    Volume getTotalVolume() const;
    OrderSide getBuyOrSell() const;

    /// New getters for our intrusive list
    poolIndex getHeadOrder() const;
    poolIndex getTailOrder() const;

    // --- INTRUSIVE LIST ROUTING ---
    // Notice we pass the ObjectPool in so the Limit can wire the pointers together!
    void addOrder(poolIndex orderIdx, Quantity shares, ObjectPool<Order>& pool);
    void removeOrder(poolIndex orderIdx, Quantity sharesToRemove, ObjectPool<Order>& pool);

    // Volume management
    void fillVolume(Quantity executedShares);

    void print() const;
};

#endif