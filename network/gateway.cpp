#include <iostream>
#include <sys/socket.h> // Core OS socket functions
#include <netinet/in.h> // IP address structures
#include <unistd.h>     // close(), read(), write()
#include <cstring>      // memset
#include "ringBuffer.hpp"

#define PORT 8080

// Force the compiler to pack this struct tightly with zero empty padding
#pragma pack(push, 1) 
struct NetworkOrder {
    uint64_t order_id;
    uint8_t  side;       // 0 for Buy, 1 for Sell
    uint32_t shares;
    uint32_t price;
};
#pragma pack(pop)

int main() {
    // 1. Create the Socket (IPv4, TCP Stream)
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Failed to create OS socket.\n";
        return 1;
    }

    // 2. Bind the Socket to Port 8080
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Listen on localhost
    address.sin_port = htons(PORT);       // htons() fixes Endianness (byte order) for the network

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed. Port 8080 might be in use.\n";
        return 1;
    }

    // 3. Listen for connections (Allow a backlog of 3 pending connections)
    if (listen(server_fd, 3) < 0) {
        std::cerr << "Listen failed.\n";
        return 1;
    }

    std::cout << "[GATEWAY] Exchange Server listening on port " << PORT << "...\n";

    // 4. Accept a connection (WARNING: This completely blocks the thread!)
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

    if (client_socket < 0) {
        std::cerr << "Failed to accept client.\n";
        return 1;
    }

    std::cout << "[GATEWAY] Trader connected successfully!\n";

    // 5. Instantiate your lock-free queue! (Capacity must be a power of 2)
    SPSCRingBuffer<NetworkOrder, 1024> order_queue;

    // The Binary Ingress Loop
    NetworkOrder incoming_order;
    while (true) {
        // Read exactly the size of our struct (17 bytes)
        ssize_t bytes_read = read(client_socket, &incoming_order, sizeof(NetworkOrder));

        if (bytes_read <= 0) {
            std::cout << "[GATEWAY] Trader disconnected.\n";
            break;
        }

        if (bytes_read == sizeof(NetworkOrder)) {
            // We received a perfect binary order! 
            std::cout << "Received Order ID: " << incoming_order.order_id 
                    << " | Side: " << (int)incoming_order.side 
                    << " | Shares: " << incoming_order.shares 
                    << " | Price: " << incoming_order.price << "\n";

            // Push it into the lock-free tunnel for the Matching Engine thread!
            if (!order_queue.push(incoming_order)) {
                std::cerr << "[WARNING] Ring Buffer is FULL! Dropping order.\n";
            }
        } else {
            std::cerr << "[WARNING] Malformed packet received. Expected " 
                    << sizeof(NetworkOrder) << " bytes, got " << bytes_read << "\n";
        }
    }

    // 6. OS Cleanup
    close(client_socket);
    close(server_fd);
    return 0;
}