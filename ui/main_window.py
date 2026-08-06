import asyncio
import threading
import customtkinter as ctk

class MargaritaApp(ctk.CTk):
    def __init__(self, esp32_client):
        super().__init__()
        
        self.esp32_client = esp32_client
        # Start a dedicated asyncio event loop in a background thread
        self.loop = asyncio.new_event_loop()
        self._loop_thread = threading.Thread(target=self._run_loop, daemon=True)
        self._loop_thread.start()
        
        # Window configuration for full-screen touch kiosk
        self.title("Margarita Machine Kiosk")
        self.geometry("1024x768")
        # self.attributes("-fullscreen", True) # Uncomment for true kiosk mode
        
        ctk.set_appearance_mode("dark")
        
        self.create_widgets()
        # Ensure loop is stopped when the window is closed
        self.protocol("WM_DELETE_WINDOW", self.on_closing)
        
        # Hook up telemetry callback from the network client
        self.esp32_client.on_telemetry_received = self.handle_telemetry

    def create_widgets(self):
        # Title Label (row 0)
        self.title_label = ctk.CTkLabel(self, text="Automated Margarita Bar", font=("Arial", 24, "bold"))
        self.title_label.grid(row=0, column=0, columnspan=2, sticky="nsew", pady=8)

        # Container for buttons (row 1)
        self.buttons_frame = ctk.CTkFrame(self)
        self.buttons_frame.grid(row=1, column=0, columnspan=2, sticky="nsew")

        # Configure grid weights for responsiveness
        self.grid_columnconfigure(0, weight=1)
        self.grid_columnconfigure(1, weight=1)
        self.grid_rowconfigure(1, weight=3)
        self.grid_rowconfigure(2, weight=1)

        for c in (0, 1):
            self.buttons_frame.grid_columnconfigure(c, weight=1)
        for r in range(3):
            self.buttons_frame.grid_rowconfigure(r, weight=1)

        # Create six pour buttons and place in 2 columns x 3 rows
        labels = [
            ("Pour Tequila\n(2.5 oz)", 1),
            ("Pour Rum\n(2.5 oz)", 2),
            ("Pour Vodka\n(2.5 oz)", 3),
            ("Pour Whisky\n(2.5 oz)", 4),
            ("Pour Soda\n(2.5 oz)", 5),
            ("Pour Syrup\n(2.5 oz)", 6),
        ]

        self.pour_buttons = []
        for idx, (text, pump_id) in enumerate(labels):
            btn = ctk.CTkButton(
                self.buttons_frame,
                text=text,
                font=("Arial", 16),
                corner_radius=0,
                command=lambda pid=pump_id: self.on_pour_clicked(pid)
            )
            r = idx // 2
            c = idx % 2
            btn.grid(row=r, column=c, sticky="nsew", padx=8, pady=8)
            self.pour_buttons.append(btn)

        # Status Display Box (row 2)
        self.status_box = ctk.CTkTextbox(self, font=("Consolas", 14))
        self.status_box.grid(row=2, column=0, columnspan=2, sticky="nsew", padx=8, pady=8)
        self.status_box.insert("0.0", "System Initialized. Waiting for connection...\n")

    def on_pour_clicked(self, pump_id: int):
        """Triggered when a pour button is pressed."""
        self.log_message(f"Requesting pour: Pump {pump_id} (2.5 oz)...")
        
        # Run the async send method safely from the synchronous UI button callback
        asyncio.run_coroutine_threadsafe(
            self.esp32_client.send_pour(pump_id=pump_id, ounces=2.5),
            self.loop
        )

    def _run_loop(self):
        """Thread target: set and run the background asyncio loop."""
        asyncio.set_event_loop(self.loop)
        self.loop.run_forever()


    def on_closing(self):
        """Stop background loop and close the UI window."""
        try:
            self.loop.call_soon_threadsafe(self.loop.stop)
            self._loop_thread.join(timeout=1)
        except Exception:
            pass
        self.destroy()

    def handle_telemetry(self, data):
        """Called asynchronously when the ESP32 sends data back."""
        self.log_message(f"ESP32 Update: {data}")

    def log_message(self, message):
        self.status_box.insert("end", f"{message}\n")
        self.status_box.see("end")