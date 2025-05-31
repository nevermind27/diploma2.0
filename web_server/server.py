from http.server import HTTPServer, SimpleHTTPRequestHandler
import os

class CORSRequestHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        super().end_headers()

    def do_GET(self):
        # Устанавливаем корневую директорию на уровень выше
        os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        return super().do_GET()

def run_server(port=8000):
    server_address = ('', port)
    httpd = HTTPServer(server_address, CORSRequestHandler)
    print(f"Сервер запущен на http://localhost:{port}")
    httpd.serve_forever()

if __name__ == "__main__":
    run_server() 