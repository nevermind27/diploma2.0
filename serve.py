from http.server import HTTPServer, SimpleHTTPRequestHandler
import os

class CORSRequestHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        super().end_headers()

# Устанавливаем рабочую директорию
os.chdir(os.path.dirname(os.path.abspath(__file__)))

# Запускаем сервер
server_address = ('', 8000)
httpd = HTTPServer(server_address, CORSRequestHandler)
print("Сервер запущен на http://localhost:8000")
httpd.serve_forever() 