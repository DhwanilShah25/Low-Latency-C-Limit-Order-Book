#pragma once
#include <atomic>
#include <vector>
#include<optional>
#include "../orderBook/order.hpp"

template<typename T, size_t Capacity>
class SPSCRingBuffer {
    // HARDWARE ENFORCEMENT: Bitwise modulo ONLY works if Capacity is a power of 2.
    static_assert((Capacity != 0) && ((Capacity & (Capacity - 1)) == 0), 
                  "Capacity must be a power of 2 for bitwise modulo to work.");

private:
    std::vector<T> buffer;

    alignas(64) std::atomic<size_t> head; 
    alignas(64) std::atomic<size_t> tail; 

public:
    SPSCRingBuffer() : buffer(Capacity), head(0), tail(0) {}

    bool push(const T& item) {
        size_t current_head = head.load(std::memory_order_relaxed);
        size_t next_head = (current_head + 1) & (Capacity - 1); // Fixed 'size' to 'Capacity'

        if(next_head == tail.load(std::memory_order_acquire)) {
            return false;
        }

        buffer[current_head] = item;
        head.store(next_head, std::memory_order_release);
        return true;
    }

    bool pop(T& outItem) {
        size_t current_tail = tail.load(std::memory_order_relaxed);

        if(current_tail == head.load(std::memory_order_acquire)) {
            return false;
        }

        outItem = buffer[current_tail];
        tail.store((current_tail + 1) & (Capacity - 1), std::memory_order_release); // Fixed 'size' to 'Capacity'
        return true;
    }

    /*
        The HFT "Time-Of-Check to Time-Of-Use" Trap
        While these functions are completely mathematically correct, there is a strict rule in 
        low-latency systems: Never use empty() or size() in your core matching loop. 
        
        Imagine your engine thread does this:

        if (!ring_buffer.empty()) {
            // What if the Gateway thread pushes an order RIGHT NOW?
            ring_buffer.pop(myOrder); 
        }

        In a multi-threaded environment, the size of the queue can change in the 1 nanosecond 
        between checking empty() and actually calling pop(). This is called a Time-Of-Check 
        to Time-Of-Use (TOCTOU) race condition.

        Instead, your engine thread should just blindly attack the pop() function in an infinite loop. 
        
        Your pop() function already contains the empty check mathematically built into it, executing it 
        in one single, perfectly safe atomic operation!

        // The Engine's Infinite Loop
        while (true) {
            Order incomingOrder;
            if (ring_buffer.pop(incomingOrder)) {
                // We got an order safely! Route it to the engine.
                book.addLimitOrder(incomingOrder.id, ...);
            }
        }

    */ 


    // size_t size() const {
    //     size_t current_head = head.load(std::memory_order_acquire);
    //     size_t current_tail = tail.load(std::memory_order_acquire);

    //     if(current_head >= current_tail) {
    //         return current_head - current_tail;
    //     } else {
    //         return Capacity + current_head - current_tail; 
    //     }
    // }

    // bool empty() const {
    //     size_t current_head = head.load(std::memory_order_acquire);
    //     size_t current_tail = tail.load(std::memory_order_acquire);

    //     return current_head == current_tail;
    // }
};