import asyncio
import sys
import threading

from core.websocket_client import ESP32Client # import esp32 client class
from ui.main_window import MargaritaApp


def run_client(esp32):
    asyncio.run(esp32.connect())


def main():
    esp32 = ESP32Client(uri="ws://192.168.4.1/ws")
    app = MargaritaApp(esp32)

    client_thread = threading.Thread(target=run_client, args=(esp32,), daemon=True)
    client_thread.start()

    app.mainloop()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("Application closed by user.")
        sys.exit(0)