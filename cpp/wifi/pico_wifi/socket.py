import asyncio
import websockets

async def handler(ws):
    print("Client connected")
    await ws.send("Hello from PC")
    async for msg in ws:
        print("Received:", msg)

asyncio.run(websockets.serve(handler, "0.0.0.0", 8080))
