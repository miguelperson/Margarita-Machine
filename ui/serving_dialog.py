import customtkinter as ctk


class ServingAmountDialog(ctk.CTkToplevel):
    """Modal dialog for selecting a serving amount (touch-only).

    Provides a live numeric display and up/down buttons that increment
    or decrement the value by 0.5 oz. Returns the chosen float in
    `self.result` when OK is pressed, otherwise `None`.
    """

    def __init__(self, master, title: str = "Select Amount", default: float = 1.5):
        super().__init__(master)

        self.title(title)
        self.resizable(False, False)
        self.result = None

        # Value state
        self._value = float(default)
        self._step = 0.5
        self._min = 0.5
        self._max = 32.0

        self._build_ui(title)

        # Center over parent and make modal
        self.transient(master)
        self.grab_set()

    def _build_ui(self, title_text: str):
        pad = 12
        lbl = ctk.CTkLabel(self, text=title_text, font=("Arial", 18, "bold"))
        lbl.grid(row=0, column=0, columnspan=3, padx=pad, pady=(pad, 4))

        # Amount display
        self.amount_label = ctk.CTkLabel(self, text=self._format(), font=("Arial", 36))
        self.amount_label.grid(row=1, column=0, columnspan=3, padx=pad, pady=4)

        # Up / Down buttons (very large for touch)
        down_btn = ctk.CTkButton(self, text="-", font=("Arial", 40), width=160, height=140,
                     command=self._decrement)
        down_btn.grid(row=2, column=0, padx=(pad, 6), pady=12)

        spacer = ctk.CTkFrame(self, width=24)
        spacer.grid(row=2, column=1)

        up_btn = ctk.CTkButton(self, text="+", font=("Arial", 40), width=160, height=140,
                       command=self._increment)
        up_btn.grid(row=2, column=2, padx=(6, pad), pady=12)

        # OK / Cancel (large)
        ok_btn = ctk.CTkButton(self, text="OK", command=self._on_ok, width=180, height=72, font=("Arial", 20))
        ok_btn.grid(row=3, column=0, columnspan=1, sticky="ew", padx=pad, pady=(12, pad))

        cancel_btn = ctk.CTkButton(self, text="Cancel", fg_color="#777777", command=self._on_cancel, width=180, height=72, font=("Arial", 20))
        cancel_btn.grid(row=3, column=2, columnspan=1, sticky="ew", padx=pad, pady=(12, pad))

    def _format(self) -> str:
        return f"{self._value:.1f} oz"

    def _increment(self):
        new = round(self._value + self._step, 2)
        if new <= self._max:
            self._value = new
            self.amount_label.configure(text=self._format())

    def _decrement(self):
        new = round(self._value - self._step, 2)
        if new >= self._min:
            self._value = new
            self.amount_label.configure(text=self._format())

    def _on_ok(self):
        self.result = self._value
        self.destroy()

    def _on_cancel(self):
        self.result = None
        self.destroy()
