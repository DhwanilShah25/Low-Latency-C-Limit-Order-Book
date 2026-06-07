import socket
import struct
import time

# Connect to the C++ Gateway
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(('127.0.0.1', 8080))
print("Connected to C++ Exchange!")

order_id = 1
try:
    while True:
        # Send a BID (Side=0) at $149.00
        bot_bid = struct.pack('<QBII', order_id, 0, 100, 14900)
        s.sendall(bot_bid)
        print(f"Sent BID Order {order_id}")
        order_id += 1
        
        # Send an ASK (Side=1) at $151.00
        bot_ask = struct.pack('<QBII', order_id, 1, 100, 15100)
        s.sendall(bot_ask)
        print(f"Sent ASK Order {order_id}")
        order_id += 1
        
        time.sleep(0.5)

except KeyboardInterrupt:
    print("Bot shutting down.")
    s.close()