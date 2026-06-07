#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include "../OrderBook/book.hpp"         // Your matching engine!
#include "ringBuffer.hpp"  // Your lock-free tunnel!

#define PORT 8080

// The Binary Network Struct
#pragma pack(push, 1)
struct NetworkOrder {
    uint64_t order_id;
    uint8_t  side;       // 0 for Buy, 1 for Sell
    uint32_t shares;
    uint32_t price;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MarketUpdate {
    uint64_t order_id;
    uint32_t last_traded_price;
    uint32_t top_bid;
    uint32_t top_ask;
};
#pragma pack(pop)

// Instantiate the Lock-Free Queue (Global so both threads can see it)
SPSCRingBuffer<NetworkOrder, 1024> order_queue;

// Instantiate the Order Book for Apple (AAPL)
Book aapl_book;

// ========================================================
// THREAD 2: THE MATCHING ENGINE
// ========================================================
void engine_loop() {
    std::cout << "[ENGINE] Booted up and spinning on dedicated CPU Core...\n";
    
    // --- 1. SETUP UDP BROADCAST SOCKET ---
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
        std::cerr << "[ENGINE] Failed to create UDP socket.\n";
        return;
    }

    // Allow broadcasting to the local network
    int broadcast_enable = 1;
    setsockopt(udp_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));

    // Set up the destination address (Broadcast to everyone on Port 8081)
    sockaddr_in broadcast_addr{};
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST); // Blast to all local IPs
    broadcast_addr.sin_port = htons(8081);                    // Different port than the TCP Gateway!

    NetworkOrder pending_order;
    MarketUpdate ticker_data;
    
    // --- 2. THE HFT INFINITE SPIN-LOOP ---
    while (true) {
        if (order_queue.pop(pending_order)) {
            OrderSide mapped_side = (pending_order.side == 0) ? OrderSide::Buy : OrderSide::Sell;
            
            // 1. Process the Order
            aapl_book.addLimitOrder(
                pending_order.order_id, 
                mapped_side, 
                pending_order.shares, 
                pending_order.price
            );
            
            // 2. Prepare the Market Update (Assuming your FlatMap has these getters, adjust if needed)
            // Note: If you don't have get_first() exposed globally yet, you can just hardcode these 
            // for the test to prove the UDP pipe works!
            ticker_data.order_id = pending_order.order_id;
            ticker_data.last_traded_price = pending_order.price; 
            ticker_data.top_bid = aapl_book.getBestBid();
            ticker_data.top_ask = aapl_book.getBestAsk();

            // 3. BLAST THE DATA OVER UDP! (Zero blocking, fire and forget)
            sendto(udp_socket, &ticker_data, sizeof(MarketUpdate), 0, 
                   (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
            
            // std::cout << "[ENGINE] Executed Order ID: " << pending_order.order_id << " | UDP Sent!\n";
        }
    }
}

// ========================================================
// THREAD 1: THE TCP GATEWAY (Main)
// ========================================================
int main() {
    // 1. Spawn the Matching Engine on a completely separate background thread
    std::thread engine_thread(engine_loop);

    // 2. Set up the OS Socket (Exactly what you just built)
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 3);
    
    std::cout << "[GATEWAY] Exchange Server listening on port " << PORT << "...\n";
    
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    
    std::cout << "[GATEWAY] Trader connected successfully!\n";

    // 3. The Ingress Loop
    NetworkOrder incoming_order;
    while (true) {
        ssize_t bytes_read = read(client_socket, &incoming_order, sizeof(NetworkOrder));

        if (bytes_read <= 0) break;

        if (bytes_read == sizeof(NetworkOrder)) {
            std::cout << "[GATEWAY] Caught network packet. Pushing to Ring Buffer...\n";
            order_queue.push(incoming_order);
        }
    }

    // 4. Cleanup
    close(client_socket);
    close(server_fd);
    
    // Join the engine thread (it runs forever, but this keeps C++ happy)
    engine_thread.join(); 
    return 0;
}