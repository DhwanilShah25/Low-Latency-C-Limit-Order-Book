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
    // For O(1) STOP ORDER LOOKUP
    std::unordered_map<ID, poolIndex> stopOrderMap;

    // Stop Order Maps
    FlatMap<std::pair<poolIndex, poolIndex>, true> stopBids;
    FlatMap<std::pair<poolIndex, poolIndex>, false> stopAsks;
    // std::map<Price, std::pair<poolIndex, poolIndex>, std::greater<Price>> stopBids;
    // std::map<Price, std::pair<poolIndex, poolIndex>> stopAsks;

    // Private Engine Helpers
    void executeStopOrders(Price currentMarketPrice, OrderSide side);
    void deleteFromOrderMap(ID orderId);

public:
    Book();
    ~Book() = default;

    // Core Matching Engine API
    void addLimitOrder(ID orderId, OrderSide buyOrSell, Quantity shares, Price limitPrice);
    void cancelLimitOrder(ID orderId);
    void modifyLimitOrder(ID orderId, Quantity newShares, Price newLimitPrice);
    void marketOrder(ID orderId, OrderSide buyOrSell, Quantity shares);

    // Stop Orders API
    void addStopOrder(ID orderId, OrderSide buyOrSell, Quantity shares, Price stopPrice);
    void cancelStopOrder(ID orderId);
    void modifyStopOrder(ID orderId, Quantity newShares, Price newStopPrice);

    // Stop Limit Orders API
    void addStopLimitOrder(ID orderId, OrderSide buyOrSell, Quantity shares, Price limitPrice, Price stopPrice);
    void cancelStopLimitOrder(ID orderId);
    void modifyStopLimitOrder(ID orderId, Quantity newShares, Price newLimitPrice, Price newStopPrice);

    // Visualisation
    void printLimit(Price limitPrice, OrderSide buyOrSell) const;
    void printOrder(ID orderId) const;
    void printOrderBook() const;

    // reset
    void reset();
};

#endif