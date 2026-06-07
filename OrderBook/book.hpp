#pragma once

#ifndef BOOK_HPP
#define BOOK_HPP

#include"types.hpp"
#include"limit.hpp"
#include"order.hpp"
#include "pool.hpp"
#include "flatmap.hpp"

// Standard Libraries
#include <cstdint> // For strict width integer types
#include<iostream>
#include<map>
#include<unordered_map>
#include <list>
#include <optional>


class Book {
private:
    
    // Instantiate your massive memory pool here (e.g., 10 million capacity)
    ObjectPool<Order> orderPool{10000000};
    
    // Limit Order Maps
    // std::map<Price, Limit, std::greater<Price>> bids;
    // std::map<Price, Limit> asks;

    FlatMap<Limit, true> bids;
    FlatMap<Limit, false> asks;
    
    // ID pointing mapped to the Pointer of that order in the order list at particular limit level
    // For O(1) ORDER LOOKUP 
    std::unordered_map<ID, poolIndex> orderMap;

    // Private Engine Helpers
    void deleteFromOrderMap(ID orderId);

public:
    Book();
    ~Book() = default;

    // Core Matching Engine API
    void addLimitOrder(ID orderId, OrderSide buyOrSell, Quantity shares, Price limitPrice);
    void cancelLimitOrder(ID orderId);
    void modifyLimitOrder(ID orderId, Quantity newShares, Price newLimitPrice);
    void marketOrder(ID orderId, OrderSide buyOrSell, Quantity shares);

    // Visualisation
    void printLimit(Price limitPrice, OrderSide buyOrSell) const;
    void printOrder(ID orderId) const;
    void printOrderBook() const;

    // Getters
    Price getBestBid() const {
        int32_t p = bids.get_first();
        return (p == -1) ? 0 : static_cast<Price>(p);
    }
    
    Price getBestAsk() const {
        int32_t p = asks.get_first();
        return (p == -1) ? 0 : static_cast<Price>(p);
    }

    // reset
    void reset();
};

#endif