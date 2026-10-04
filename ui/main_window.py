import asyncio
import threading
import customtkinter as ctk
from .serving_dialog import ServingAmountDialog

class MargaritaApp(ctk.CTk):
    MAX_DRINK_OUNCES = 16.0

    @staticmethod
    def get_total_selected_oz(selections):
        return sum(float(item.get("amount", 0) or 0) for item in selections)

    @classmethod
    def has_overflow(cls, selections, limit=None):
        limit = cls.MAX_DRINK_OUNCES if limit is None else float(limit)
        return cls.get_total_selected_oz(selections) > limit

    def __init__(self, esp32_client):
        super().__init__()
        
        self.esp32_client = esp32_client
        self.selections = []  # list of {'name', 'pump_id', 'amount'}
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
        self.grid_rowconfigure(3, weight=1)

        for c in (0, 1):
            self.buttons_frame.grid_columnconfigure(c, weight=1)
        for r in range(3):
            self.buttons_frame.grid_rowconfigure(r, weight=1)

        # Create six pour buttons and place in 2 columns x 3 rows
        labels = [
            ("Pour Tequila", 1),
            ("Pour Rum", 2),
            ("Pour Vodka", 3),
            ("Pour Whisky", 4),
            ("Pour Soda", 5),
            ("Pour Syrup", 6),
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

        # Control frame: selections and serve button (row 2)
        self.control_frame = ctk.CTkFrame(self)
        self.control_frame.grid(row=2, column=0, columnspan=2, sticky="nsew", padx=8, pady=4)

        # Selections tappable list (scrollable)
        self.selection_frame = ctk.CTkScrollableFrame(self.control_frame, height=160)
        self.selection_frame.grid(row=0, column=0, columnspan=3, sticky="nsew", padx=8, pady=8)
        self.control_frame.grid_columnconfigure(0, weight=1)
        self.selection_frame.grid_columnconfigure(0, weight=1)

        # initial empty state
        self.empty_label = ctk.CTkLabel(self.selection_frame, text="No selections yet.")
        self.empty_label.grid(row=0, column=0, padx=8, pady=8)

        # Serve button aligned on the right
        self.serve_btn = ctk.CTkButton(self.control_frame, text="Serve", fg_color="#1f6feb", command=self.serve_all, width=160, height=64, font=("Arial", 18))
        self.serve_btn.grid(row=1, column=2, sticky="e", padx=8, pady=8)

        # Status Display Box (row 3)
        self.status_box = ctk.CTkTextbox(self, font=("Consolas", 14))
        self.status_box.grid(row=3, column=0, columnspan=2, sticky="nsew", padx=8, pady=8)
        self.status_box.insert("0.0", "System Initialized. Waiting for connection...\n")

    def on_pour_clicked(self, pump_id: int):
        """Open the serving-size dialog and add the selection to the list."""
        # Map pump id to friendly name
        pump_names = {1: "Tequila", 2: "Rum", 3: "Vodka", 4: "Whisky", 5: "Soda", 6: "Syrup"}
        name = pump_names.get(pump_id, f"Pump {pump_id}")

        dlg = ServingAmountDialog(self, title=name, default=1.5)
        self.wait_window(dlg)
        if getattr(dlg, "result", None):
            amount = dlg.result
            if self.add_selection(name=name, pump_id=pump_id, amount=amount):
                self.log_message(f"Queued: {name} — {amount:.1f} oz")

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

    def add_selection(self, name: str, pump_id: int, amount: float):
        amount = float(amount)
        trial_selections = [dict(item) for item in self.selections] + [{"name": name, "pump_id": pump_id, "amount": amount}]
        if self.has_overflow(trial_selections, self.MAX_DRINK_OUNCES):
            total = self.get_total_selected_oz(self.selections) + amount
            self.log_message(
                f"Drink overflow: {total:.1f} oz exceeds the 16 oz limit. "
                "Please reduce the amount before serving."
            )
            return False

        self.selections.append({"name": name, "pump_id": pump_id, "amount": amount})
        self.update_selection_display()
        return True

    def update_selection_display(self):
        # Clear previous widgets
        for w in self.selection_frame.winfo_children():
            w.destroy()

        if not self.selections:
            self.empty_label = ctk.CTkLabel(self.selection_frame, text="No selections yet.")
            self.empty_label.grid(row=0, column=0, padx=8, pady=8)
            return

        for idx, s in enumerate(self.selections):
            row_frame = ctk.CTkFrame(self.selection_frame)
            row_frame.grid(row=idx, column=0, sticky="ew", padx=6, pady=6)
            row_frame.grid_columnconfigure(0, weight=1)

            lbl = ctk.CTkLabel(row_frame, text=f"{s['name']} — {s['amount']:.1f} oz", anchor="w", font=("Arial", 16))
            lbl.grid(row=0, column=0, sticky="w", padx=(8, 4))

            edit_btn = ctk.CTkButton(row_frame, text="Edit", width=80, height=36, command=lambda i=idx: self.edit_selection(i))
            edit_btn.grid(row=0, column=1, padx=4, pady=4)

            del_btn = ctk.CTkButton(row_frame, text="Delete", fg_color="#c0392b", width=80, height=36, command=lambda i=idx: self.delete_selection(i))
            del_btn.grid(row=0, column=2, padx=4, pady=4)

    def remove_last_selection(self):
        # Deprecated: use per-item delete buttons instead
        if not self.selections:
            return
        last = self.selections.pop()
        self.update_selection_display()
        self.log_message(f"Removed selection: {last['name']} {last['amount']:.1f} oz")

    def edit_last_selection(self):
        # Deprecated: use per-item edit buttons instead
        if not self.selections:
            return
        last = self.selections[-1]
        dlg = ServingAmountDialog(self, title=last['name'], default=last['amount'])
        self.wait_window(dlg)
        if getattr(dlg, "result", None):
            updated_selection = [dict(s) for s in self.selections]
            updated_selection[-1]["amount"] = dlg.result
            if self.has_overflow(updated_selection, self.MAX_DRINK_OUNCES):
                self.log_message(
                    f"Drink overflow: {self.get_total_selected_oz(updated_selection):.1f} oz exceeds the 16 oz limit. "
                    "Please reduce the amount before serving."
                )
                return

            last['amount'] = dlg.result
            self.update_selection_display()
            self.log_message(f"Updated selection: {last['name']} — {last['amount']:.1f} oz")

    def edit_selection(self, index: int):
        if index < 0 or index >= len(self.selections):
            return
        item = self.selections[index]
        dlg = ServingAmountDialog(self, title=item['name'], default=item['amount'])
        self.wait_window(dlg)
        if getattr(dlg, "result", None):
            updated_selection = [dict(s) for s in self.selections]
            updated_selection[index]["amount"] = dlg.result
            if self.has_overflow(updated_selection, self.MAX_DRINK_OUNCES):
                self.log_message(
                    f"Drink overflow: {self.get_total_selected_oz(updated_selection):.1f} oz exceeds the 16 oz limit. "
                    "Please reduce the amount before serving."
                )
                return

            item['amount'] = dlg.result
            self.update_selection_display()
            self.log_message(f"Updated selection: {item['name']} — {item['amount']:.1f} oz")

    def delete_selection(self, index: int):
        if index < 0 or index >= len(self.selections):
            return
        item = self.selections.pop(index)
        self.update_selection_display()
        self.log_message(f"Deleted selection: {item['name']} — {item['amount']:.1f} oz")

    def serve_all(self):
        if not self.selections:
            self.log_message("No selections to serve.")
            return

        total_oz = self.get_total_selected_oz(self.selections)
        if total_oz > self.MAX_DRINK_OUNCES:
            self.log_message(
                f"Cannot serve drink: total is {total_oz:.1f} oz, which exceeds the 16 oz limit."
            )
            return

        payload = []
        for s in self.selections:
            payload.append({"ingredient": s['name'], "pump": s['pump_id'], "oz": s['amount']})

        # Send as an aggregated serve command over websocket
        asyncio.run_coroutine_threadsafe(
            self.esp32_client.send_serve(payload),
            self.loop
        )
        self.log_message(f"Sent serve command with {len(self.selections)} items.")
        self.selections = []
        self.update_selection_display()