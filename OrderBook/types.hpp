#pragma once

#ifndef TYPES_HPP
#define TYPES_HPP

// Standard Libraries
#include <cstdint> // For strict width integer types
#include <cstddef> // FIX: Added for size_t

using Price = uint32_t;
using Size = uint32_t;
using Volume = uint64_t;

// enum so that no one accidently pushes price in OrderSide
enum class OrderSide : uint8_t { 
    Buy, 
    Sell 
};

enum class OrderType : uint8_t {
    Limit,
    Market
};

using ID = uint64_t;
using Quantity = uint32_t;

using poolIndex = uint32_t;
using poolCapacity = size_t;
#endif