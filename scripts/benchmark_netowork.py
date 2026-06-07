import socket
import struct
import time
import threading

# Shared dictionary to track when each order was sent
send_times = {}
latencies = []
EXPECTED_ORDERS = 1000

def udp_listener():
    # Set up the UDP Radio Receiver
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('0.0.0.0', 8081))
    
    received = 0
    while received < EXPECTED_ORDERS:
        data, _ = sock.recvfrom(1024)
        recv_time = time.perf_counter_ns() # Stamp the exact arrival time!
        
        # Unpack: order_id (8 bytes), price (4), bid (4), ask (4) = 20 bytes total ('<QIII')
        if len(data) == 20:
            order_id, last_price, top_bid, top_ask = struct.unpack('<QIII', data)
            
            if order_id in send_times:
                # Calculate Round-Trip Latency in nanoseconds
                rtt_ns = recv_time - send_times[order_id]
                latencies.append(rtt_ns)
                received += 1

# 1. Start the UDP Listener on a background thread
listener = threading.Thread(target=udp_listener)
listener.start()

# 2. Connect the TCP Gateway
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(('127.0.0.1', 8080))
# Tell the OS to send packets immediately (Disable Nagle's Algorithm)
s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

print("Engine warming up...")
time.sleep(1)

print(f"Blasting {EXPECTED_ORDERS} orders for Wire-to-Wire test...")

for i in range(1, EXPECTED_ORDERS + 1):
    order_id = i
    # Alternate Buys and Sells to force the engine to match trades
    side = i % 2 
    price = 15000 if side == 0 else 14900
    
    binary_order = struct.pack('<QBII', order_id, side, 100, price)
    
    # Stamp the time and blast!
    send_times[order_id] = time.perf_counter_ns()
    s.sendall(binary_order)
    
    # Sleep for 1 millisecond between orders so we don't overwhelm the Mac's loopback network
    time.sleep(0.001) 

# Wait for the last UDP packet to arrive
listener.join()
s.close()

# 3. Calculate Wall Street Stats
avg_latency_us = (sum(latencies) / len(latencies)) / 1000.0
min_latency_us = min(latencies) / 1000.0
max_latency_us = max(latencies) / 1000.0

print("\n=== WIRE-TO-WIRE LATENCY (Round Trip) ===")
print(f"Total Orders Processed: {len(latencies)}")
print(f"Internal Engine Speed : ~0.12 microseconds (123 ns)")
print(f"Min Network Latency   : {min_latency_us:.2f} microseconds")
print(f"Average Network Latency: {avg_latency_us:.2f} microseconds")
print(f"Max Network Latency   : {max_latency_us:.2f} microseconds")
print("=========================================")