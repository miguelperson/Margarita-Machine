import asyncio
import json
import os
import subprocess
import tempfile

import websockets


class ESP32Client:
    def __init__(
        self,
        uri="ws://192.168.4.1/ws",
        ssid="MargaritaMachine",
        password="12345678",
    ):
        self.uri = uri
        self.ssid = ssid
        self.password = password
        self.websocket = None
        self.is_connected = False
        self.on_telemetry_received = None

    def ensure_wifi_connected(self):
        """Connect the Windows PC to the ESP32's Wi-Fi access point if needed."""
        try:
            result = subprocess.run(
                ["netsh", "wlan", "show", "interfaces"],
                capture_output=True,
                text=True,
                check=False,
            )
            if result.returncode == 0 and self.ssid.lower() in result.stdout.lower():
                print(f"Wi-Fi already connected to {self.ssid}.")
                return

            profile_xml = f'''<?xml version="1.0"?>
<WLANProfile xmlns="http://www.microsoft.com/networking/WLAN/profile/v1">
    <name>{self.ssid}</name>
    <SSIDConfig>
        <SSID>
            <name>{self.ssid}</name>
        </SSID>
    </SSIDConfig>
    <connectionType>ESS</connectionType>
    <connectionMode>manual</connectionMode>
    <MSM>
        <security>
            <authEncryption>
                <authentication>WPA2PSK</authentication>
                <encryption>AES</encryption>
                <useOneX>false</useOneX>
            </authEncryption>
            <sharedKey>
                <keyType>passPhrase</keyType>
                <protected>false</protected>
                <keyMaterial>{self.password}</keyMaterial>
            </sharedKey>
        </security>
    </MSM>
</WLANProfile>'''

            with tempfile.NamedTemporaryFile("w", suffix=".xml", delete=False, encoding="utf-8") as profile_file:
                profile_file.write(profile_xml)
                temp_profile_path = profile_file.name

            subprocess.run(
                ["netsh", "wlan", "delete", "profile", f"name={self.ssid}"],
                capture_output=True,
                text=True,
                check=False,
            )
            subprocess.run(
                ["netsh", "wlan", "add", "profile", f"filename={temp_profile_path}"],
                capture_output=True,
                text=True,
                check=False,
            )
            os.unlink(temp_profile_path)

            connect_result = subprocess.run(
                ["netsh", "wlan", "connect", f"name={self.ssid}"],
                capture_output=True,
                text=True,
                check=False,
            )
            if connect_result.returncode == 0:
                print(f"Connected to Wi-Fi SSID: {self.ssid}")
            else:
                print(f"Failed to connect to Wi-Fi SSID: {self.ssid}")
                print(connect_result.stderr or connect_result.stdout)

        except FileNotFoundError:
            print("Windows netsh is unavailable; this app must be running on Windows to connect to the ESP32 Wi-Fi AP.")

    async def connect(self):
        self.ensure_wifi_connected()

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

    async def send_serve(self, items: list):
        """Send an aggregated serve command. `items` should be a list of dicts
        with keys like `ingredient`, `pump`, and `oz`.
        """
        await self.send_command("serve", {"items": items})
