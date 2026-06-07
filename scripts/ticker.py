import socket
import struct

# Create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Bind it to port 8081 so it listens for the Engine's broadcasts
sock.bind(('0.0.0.0', 8081))
print("Listening for UDP Market Data on port 8081...")

try:
    while True:
        # Receive up to 1024 bytes (our struct is only 12 bytes)
        data, addr = sock.recvfrom(1024)
        
        if len(data) == 12:
            # Unpack the 3 uint32_t variables (Little Endian format '<III')
            last_price, top_bid, top_ask = struct.unpack('<III', data)
            print(f"[TICK] Last Traded: ${last_price/100:.2f} | Spread: ${top_bid/100:.2f} x ${top_ask/100:.2f}")

except KeyboardInterrupt:
    print("Ticker stopped.")
    sock.close()