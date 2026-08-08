import importlib
m = importlib.import_module('core.websocket_client')
print('ESP32Client' in dir(m))
m2 = importlib.import_module('main')
print(callable(m2.main))
