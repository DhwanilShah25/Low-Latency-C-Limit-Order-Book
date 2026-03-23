#pragma once

#ifndef FLATMAP_HPP
#define FLATMAP_HPP

#include "types.hpp"
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

// Template accepts the value type, the iteration direction, and the maximum allowed price.
template <typename T, bool IsBid, size_t MAX_PRICE = 200000>
class FlatMap {
public:
    static constexpr size_t WORDS = (MAX_PRICE / 64) + 1;

    // Mimics the std::pair that std::map iterators return
    struct Node {
        Price first;
        T second;
        Node(Price p, T val) : first(p), second(std::move(val)) {}
    };

    // The Flat Array Memory Block
    std::vector<std::optional<Node>> data;
    
    // The Bitmask (1 bit per price level)
    uint64_t bitmask[WORDS] = {0};
    // Constructor to pre-allocate the exact size immediately
    FlatMap() : data(MAX_PRICE) {}


    // --- FIX: O(1) Top-Of-Book Trackers ---
    int32_t min_price = MAX_PRICE;
    int32_t max_price = -1;

    inline void set_bit(Price p) { 
        bitmask[p / 64] |= (1ULL << (p % 64)); 
        if (p < min_price) min_price = p;
        if (p > max_price) max_price = p;
    }
    
    inline void clear_bit(Price p) { 
        bitmask[p / 64] &= ~(1ULL << (p % 64)); 
        // If we delete the very edge of the book, scan once to find the new edge
        if (p == min_price) min_price = scan_up(p);
        if (p == max_price) max_price = scan_down(p);
        if (min_price == -1) { min_price = MAX_PRICE; max_price = -1; }
    }
    
    inline bool get_bit(Price p) const { return bitmask[p / 64] & (1ULL << (p % 64)); }

    // --- HFT Bitmask Teleportation ---
    int32_t scan_up(int32_t p) const {
        int32_t start_idx = p + 1;
        if (start_idx >= static_cast<int32_t>(MAX_PRICE)) return -1;
        int32_t w = start_idx / 64;
        int32_t bit_idx = start_idx % 64;
        
        uint64_t mask = bitmask[w] & (~0ULL << bit_idx);
        if (mask) return w * 64 + __builtin_ctzll(mask);
        
        for (size_t i = w + 1; i < WORDS; ++i) {
            if (bitmask[i]) return i * 64 + __builtin_ctzll(bitmask[i]);
        }
        return -1;
    }

    int32_t scan_down(int32_t p) const {
        int32_t start_idx = p - 1;
        if (start_idx < 0) return -1;
        int32_t w = start_idx / 64;
        int32_t bit_idx = start_idx % 64;
        
        uint64_t mask = bitmask[w];
        if (bit_idx < 63) mask &= ((1ULL << (bit_idx + 1)) - 1);
        
        if (mask) return w * 64 + 63 - __builtin_clzll(mask);
        
        for (int32_t i = w - 1; i >= 0; --i) {
            if (bitmask[i]) return i * 64 + 63 - __builtin_clzll(bitmask[i]);
        }
        return -1;
    }

    int32_t get_next(int32_t p) const { return IsBid ? scan_down(p) : scan_up(p); }
    int32_t get_prev(int32_t p) const { return IsBid ? scan_up(p) : scan_down(p); }
    
    int32_t get_first() const { 
        if (max_price == -1) return -1; 
        return IsBid ? max_price : min_price; 
    }
    
    int32_t get_last() const { 
        if (max_price == -1) return -1; 
        return IsBid ? min_price : max_price; 
    }

    // --- STL-Compliant Iterator ---
    class Iterator {
    public:
        FlatMap* map;
        int32_t price; // -1 represents the end() of the map

        Iterator(FlatMap* m, int32_t p) : map(m), price(p) {}

        Iterator& operator++() {
            if (price != -1) price = map->get_next(price);
            return *this;
        }

        bool operator!=(const Iterator& other) const { return price != other.price; }
        bool operator==(const Iterator& other) const { return price == other.price; }

        // Allows book.cpp to call it->first and it->second exactly like std::map!
        Node* operator->() { return &map->data[price].value(); }
        Node& operator*() { return map->data[price].value(); }
    };

    // --- STL-Compliant Reverse Iterator (Used for printing) ---
    class ReverseIterator {
    public:
        FlatMap* map;
        int32_t price;

        ReverseIterator(FlatMap* m, int32_t p) : map(m), price(p) {}

        ReverseIterator& operator++() {
            if (price != -1) price = map->get_prev(price);
            return *this;
        }

        bool operator!=(const ReverseIterator& other) const { return price != other.price; }
        Node* operator->() { return &map->data[price].value(); }
    };

    // --- STL-Compliant Const Iterators ---
    class ConstIterator {
    public:
        const FlatMap* map;
        int32_t price; // -1 represents the end() of the map

        ConstIterator(const FlatMap* m, int32_t p) : map(m), price(p) {}

        ConstIterator& operator++() {
            if (price != -1) price = map->get_next(price);
            return *this;
        }

        bool operator!=(const ConstIterator& other) const { return price != other.price; }
        bool operator==(const ConstIterator& other) const { return price == other.price; }

        // Notice the 'const' return types to protect the data!
        const Node* operator->() const { return &map->data[price].value(); }
        const Node& operator*() const { return map->data[price].value(); }
    };

    class ConstReverseIterator {
    public:
        const FlatMap* map;
        int32_t price;

        ConstReverseIterator(const FlatMap* m, int32_t p) : map(m), price(p) {}

        ConstReverseIterator& operator++() {
            if (price != -1) price = map->get_prev(price);
            return *this;
        }

        bool operator!=(const ConstReverseIterator& other) const { return price != other.price; }
        const Node* operator->() const { return &map->data[price].value(); }
    };

    // --- STL Interface Methods ---
    Iterator begin() { return Iterator(this, get_first()); }
    Iterator end() { return Iterator(this, -1); }
    ReverseIterator rbegin() { return ReverseIterator(this, get_last()); }
    ReverseIterator rend() { return ReverseIterator(this, -1); }

    // --- Const STL Interface Methods (For Read-Only access) ---
    ConstIterator begin() const { return ConstIterator(this, get_first()); }
    ConstIterator end() const { return ConstIterator(this, -1); }
    ConstReverseIterator rbegin() const { return ConstReverseIterator(this, get_last()); }
    ConstReverseIterator rend() const { return ConstReverseIterator(this, -1); }

    Iterator find(Price p) {
        if (p < MAX_PRICE && get_bit(p)) return Iterator(this, p);
        return end();
    }

    std::pair<Iterator, bool> emplace(Price p, T val) {
        if (p >= MAX_PRICE) throw std::out_of_range("Price exceeds MAX_PRICE limit");
        if (!get_bit(p)) {
            data[p].emplace(p, std::move(val));
            set_bit(p);
            return {Iterator(this, p), true};
        }
        return {Iterator(this, p), false};
    }

    Iterator erase(Iterator it) {
        if (it.price == -1) return end();
        int32_t p = it.price;
        int32_t next_p = get_next(p);
        data[p].reset();
        clear_bit(p);
        return Iterator(this, next_p);
    }

    T& operator[](Price p) {
        if (p >= MAX_PRICE) throw std::out_of_range("Price exceeds MAX_PRICE limit");
        if (!get_bit(p)) {
            data[p].emplace(p, T{}); // Default constructs empty limits/pairs
            set_bit(p);
        }
        return data[p]->second;
    }

    T& at(Price p) {
        if (p >= MAX_PRICE || !get_bit(p)) throw std::out_of_range("Price not found");
        return data[p]->second;
    }
};

#endif