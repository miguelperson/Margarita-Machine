import asyncio
import json

import websockets


class ESP32Client:
    def __init__(self, uri="ws://192.168.4.1/ws"):
        self.uri = uri
        self.websocket = None
        self.is_connected = False
        self.on_telemetry_received = None

    async def connect(self):
        while True:
            try:
                print(f"Connecting to ESP32 at {self.uri}...")
                async with websockets.connect(self.uri) as websocket:
                    self.websocket = websocket
                    self.is_connected = True
                    print("Connected to ESP32!")

                    async for message in websocket:
                        if self.on_telemetry_received:
                            data = json.loads(message)
                            self.on_telemetry_received(data)
            except (websockets.ConnectionClosed, ConnectionRefusedError):
                self.is_connected = False
                print("Connection lost. Reconnecting in 2 seconds...")
                await asyncio.sleep(2)

    async def send_command(self, action: str, payload: dict):
        if not self.is_connected or not self.websocket:
            print("Error: Cannot send command, not connected.")
            return

        message = {"action": action, **payload}
        json_data = json.dumps(message)
        await self.websocket.send(json_data)
        print(f"Sent: {json_data}")

    async def send_pour(self, pump_id: int, ounces: float):
        await self.send_command("pour", {"pump": pump_id, "oz": ounces})
