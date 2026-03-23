#pragma once
#include <vector>
#include <cstdint>
#include <stdexcept>
#include<iostream>
#include"types.hpp"
#include <new>     // Required for placement new
#include <utility> // Required for perfect forwarding

template <typename T>
class ObjectPool {
private:
    // The Union: This is the secret sauce. 
    // It takes up the exact same memory as your object (T), but when the 
    // object isn't being used, we use that same memory to store the 'next' index.
    union PoolElement {
        T object;
        poolIndex next_free_index;
        
        // We have to explicitly define constructors/destructors for unions containing classes
        PoolElement() {} 
        ~PoolElement() {}
    };

    // The Memory Block: Just one single vector now. No separate stack.
    std::vector<PoolElement> pool;
    
    // The Valet: Just a single integer pointing to the first available spot.
    poolIndex head_free_index;

public:
    ObjectPool(poolCapacity capacity) {
        // 1. Resize the 'pool' to hold 'capacity' elements.
        pool.resize(capacity);
        
        // 2. Link them all together. 
        // Go through the pool and write the index of the *next* spot into the current spot.
        for (poolCapacity i = 0; i < capacity - 1; ++i) {
            pool[i].next_free_index = static_cast<int32_t>(i + 1);
        }
        
        // 3. The last spot has no "next" spot, so we give it a special marker (like -1).
        pool[capacity - 1].next_free_index = -1;
        
        // 4. Set the valet to point to the very first spot.
        head_free_index = 0;
    }

    void reset() {
        poolCapacity capacity = pool.size();
        for (poolCapacity i = 0; i < capacity - 1; ++i)
            pool[i].next_free_index = static_cast<int32_t>(i + 1);
        pool[capacity - 1].next_free_index = -1;
        head_free_index = 0;
    }

    poolIndex allocate() {
        // TODO 1: Check if 'head_free_index' is -1 (meaning the pool is completely full). Throw an error if so.
        if(head_free_index==static_cast<poolIndex>(-1)){
            throw std::runtime_error("CRITICAL: Order Pool Exhausted!");
        }

        // TODO 2: Store the current 'head_free_index' in a temporary variable. This is the spot we are giving to the user.
        poolIndex currFreeIndex = head_free_index;
        // TODO 3: Look *inside* that spot to find the next available index, and update 'head_free_index' to that number.
        head_free_index = pool[currFreeIndex].next_free_index;
        // TODO 4: Return the temporary variable.
        return currFreeIndex;
    }

    void deallocate(poolIndex index) {
        // TODO 1: Take the spot being returned (at 'index') and write the CURRENT 'head_free_index' into it.
        pool[index].next_free_index = head_free_index;
        // TODO 2: Update 'head_free_index' to be the 'index' that was just returned.
        head_free_index = index;
    }

    // ---------------------------------------------------------
    // THE FACTORY WRAPPERS
    // ---------------------------------------------------------

    // This uses a C++ template trick called "Variadic Templates".
    // It basically means "take whatever arguments the user passes in, 
    // and hand them directly to the object's constructor."
    template <typename... Args>
    poolIndex construct(Args&&... args) {
        // 1. Get a free index from the valet
        poolIndex index = allocate();
        
        // 2. Use Placement New to build the object directly into that memory slot
        new (&pool[index].object) T(std::forward<Args>(args)...);
        
        return index;
    }

    // Safely destroys the object and returns the memory to the pool
    void destroy(poolIndex index) {
        // 1. Explicitly call the object's destructor
        pool[index].object.~T();
        
        // 2. Give the index back to the valet
        deallocate(index);
    }

    const T& get(poolIndex index) const {
        return pool[index].object;
    }
    
    T& get(poolIndex index) {
        return pool[index].object;
    }
};