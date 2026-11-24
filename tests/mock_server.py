
import json
from http.server import BaseHTTPRequestHandler, HTTPServer


class MockOllamaHandler(BaseHTTPRequestHandler):

    def do_POST(self):
        if self.path != "/api/chat":
            self.send_error(404, "Not Found")
            return

        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length)

        try:
            request_data = json.loads(body.decode("utf-8"))
            print("\n[Mock Server] Received request:")
            print(json.dumps(request_data, indent=4, ensure_ascii=False))

            response_data = {
                "model": request_data.get("model", "mock-model"),
                "created_at": "2026-01-01T00:00:00Z",
                "message": {
                    "role": "assistant",
                    "content": "Hello from Mock Ollama Server!"
                },
                "done": True,
                "done_reason": "stop",
                "prompt_eval_count": 10,
                "eval_count": 8
            }

            response_body = json.dumps(response_data).encode("utf-8")

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(response_body)))
            self.end_headers()

            self.wfile.write(response_body)

            print("[Mock Server] Response sent successfully.")

        except json.JSONDecodeError:
            self.send_error(400, "Invalid JSON")

    def log_message(self, format, *args):
        print(f"[HTTP] {format % args}")


def main():
    host = "127.0.0.1"
    port = 8000

    server = HTTPServer((host, port), MockOllamaHandler)

    print(f"[Mock Server] Running at http://{host}:{port}")
    print("[Mock Server] Waiting for requests...")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[Mock Server] Shutting down...")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
