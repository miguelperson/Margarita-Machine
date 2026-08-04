import asyncio
import customtkinter as ctk

class MargaritaApp(ctk.CTk):
    def __init__(self, esp32_client):
        super().__init__()
        
        self.esp32_client = esp32_client
        
        # Window configuration for full-screen touch kiosk
        self.title("Margarita Machine Kiosk")
        self.geometry("1024x768")
        # self.attributes("-fullscreen", True) # Uncomment for true kiosk mode
        
        ctk.set_appearance_mode("dark")
        
        self.create_widgets()
        
        # Hook up telemetry callback from the network client
        self.esp32_client.on_telemetry_received = self.handle_telemetry

    def create_widgets(self):
        # Title Label
        self.title_label = ctk.CTkLabel(self, text="Automated Margarita Bar", font=("Arial", 28, "bold"))
        self.title_label.pack(pady=30)

        # Pour Button (Classic Margarita - Pump 1, 2.5 oz)
        self.pour_btn = ctk.CTkButton(
            self, 
            text="Pour Classic Margarita\n(2.5 oz)", 
            font=("Arial", 20),
            width=300, 
            height=120,
            corner_radius=15,
            command=self.on_pour_clicked
        )
        self.pour_btn.pack(pady=40)

        # Status Display Box
        self.status_box = ctk.CTkTextbox(self, width=600, height=200, font=("Consolas", 14))
        self.status_box.pack(pady=20)
        self.status_box.insert("0.0", "System Initialized. Waiting for connection...\n")

    def on_pour_clicked(self):
        """Triggered when the touch button is pressed."""
        self.log_message("Requesting pour: Pump 1 (2.5 oz)...")
        
        # Run the async send method safely from the synchronous UI button callback
        asyncio.run_coroutine_threadsafe(
            self.esp32_client.send_pour(pump_id=1, ounces=2.5), 
            asyncio.get_event_loop()
        )

    def handle_telemetry(self, data):
        """Called asynchronously when the ESP32 sends data back."""
        self.log_message(f"ESP32 Update: {data}")

    def log_message(self, message):
        self.status_box.insert("end", f"{message}\n")
        self.status_box.see("end")