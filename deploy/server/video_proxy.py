#!/usr/bin/env python3
import asyncio
import websockets
import sys

PORT = 9002
subscribers = set()
publisher = None

async def handler(websocket, path):
    global publisher
    if path == "/publish":
        if publisher is not None:
            try:
                await publisher.close()
            except Exception:
                pass
        publisher = websocket
        print(f"[PROXY] ESP32-CAM conectado desde {websocket.remote_address}")
        frames_received = 0

        async def safe_send(sub, data):
            try:
                await sub.send(data)
            except Exception:
                subscribers.discard(sub)

        try:
            async for message in websocket:
                frames_received += 1
                if frames_received % 50 == 0:
                    print(f"[PROXY] Frames: {frames_received} | Viewers: {len(subscribers)}")
                if subscribers:
                    for sub in list(subscribers):
                        asyncio.create_task(safe_send(sub, message))
        except websockets.exceptions.ConnectionClosed:
            pass
        finally:
            if publisher == websocket:
                publisher = None
            print(f"[PROXY] ESP32-CAM desconectado. Frames: {frames_received}")
    else:
        print(f"[PROXY] Viewer conectado desde {websocket.remote_address}")
        subscribers.add(websocket)
        try:
            async for _ in websocket:
                pass
        except websockets.exceptions.ConnectionClosed:
            pass
        finally:
            subscribers.discard(websocket)
            print(f"[PROXY] Viewer desconectado ({websocket.remote_address})")

async def main():
    print(f"[PROXY] Iniciando en ws://0.0.0.0:{PORT} ...")
    async with websockets.serve(
        handler, "0.0.0.0", PORT,
        max_size=10 * 1024 * 1024,
        ping_interval=None, ping_timeout=None,
    ):
        print(f"[PROXY] Escuchando en el puerto {PORT}.")
        await asyncio.Future()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n[PROXY] Detenido.")
