#!/usr/bin/env python3
import serial
import asyncio
import websockets
import json

PORT = '/dev/cu.usbmodem101'
BAUD = 115200
clients = set()

async def read_serial(ser):
    loop = asyncio.get_event_loop()
    while True:
        try:
            if ser.in_waiting:
                line = await loop.run_in_executor(None, ser.readline)
                line = line.decode('utf-8').strip()
                
                print(line)  # PRINT EVERYTHING
                
                if line.startswith("HR:"):
                    parts = line.split(',')
                    data = {}
                    for part in parts:
                        k, v = part.split(':')
                        data[k.lower()] = int(v)
                    
                    if clients:
                        msg = json.dumps(data)
                        await asyncio.gather(*[c.send(msg) for c in clients], return_exceptions=True)
                        
            await asyncio.sleep(0.01)
        except:
            await asyncio.sleep(0.1)

async def handle_client(websocket):
    clients.add(websocket)
    print(f"Client connected ({len(clients)} total)")
    try:
        await websocket.wait_closed()
    finally:
        clients.remove(websocket)
        print(f"Client disconnected ({len(clients)} remaining)")

async def main():
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print(f"Connected to {PORT}")
    
    async with websockets.serve(handle_client, "localhost", 8765):
        print("Server running on ws://localhost:8765")
        print("Open heartrate.html in browser\n")
        await read_serial(ser)

if __name__ == "__main__":
    asyncio.run(main())