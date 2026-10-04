import importlib
import unittest


class ImportTests(unittest.TestCase):
    def test_core_client_import(self):
        module = importlib.import_module("core.websocket_client")
        self.assertTrue(hasattr(module, "ESP32Client"))

    def test_main_module_imports(self):
        import main

        self.assertTrue(callable(main.main))

    def test_drink_oz_limit_checker(self):
        from ui.main_window import MargaritaApp

        selections = [{"amount": 8.0}, {"amount": 8.5}]
        self.assertEqual(MargaritaApp.get_total_selected_oz(selections), 16.5)
        self.assertTrue(MargaritaApp.has_overflow(selections, 16.0))

        self.assertFalse(MargaritaApp.has_overflow([{"amount": 8.0}, {"amount": 8.0}], 16.0))


if __name__ == "__main__":
    unittest.main()
