import importlib
import unittest


class ImportTests(unittest.TestCase):
    def test_core_client_import(self):
        module = importlib.import_module("core.websocket_client")
        self.assertTrue(hasattr(module, "ESP32Client"))

    def test_main_module_imports(self):
        import main

        self.assertTrue(callable(main.main))


if __name__ == "__main__":
    unittest.main()
