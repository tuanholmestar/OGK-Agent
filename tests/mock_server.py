

import json
from http.server import BaseHTTPRequestHandler, HTTPServer


HOST = "127.0.0.1"
PORT = 8000


class MockOllamaHandler(BaseHTTPRequestHandler):

    def send_json(self, status_code, data):
        response_body = json.dumps(data).encode("utf-8")

        self.send_response(status_code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(response_body)))
        self.end_headers()

        self.wfile.write(response_body)

    def send_raw(self, status_code, data):
        response_body = data.encode("utf-8")

        self.send_response(status_code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(response_body)))
        self.end_headers()

        self.wfile.write(response_body)

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

            model = request_data.get("model", "")

            # Mode 1: Echo request để kiểm tra serialization.
            if model == "mock-echo-request":
                response_data = {
                    "model": model,
                    "created_at": "2026-01-01T00:00:00Z",
                    "message": {
                        "role": "assistant",
                        "content": json.dumps(
                            request_data,
                            ensure_ascii=False
                        )
                    },
                    "done": True,
                    "done_reason": "stop"
                }

                self.send_json(200, response_data)

            # Mode 2: Giả lập HTTP 500.
            elif model == "mock-http-error":
                self.send_json(500, {
                    "error": "Internal Server Error"
                })

            # Mode 3: Giả lập response JSON không hợp lệ.
            elif model == "mock-malformed-json":
                self.send_raw(200, "{ invalid json")

            # Mode 4: JSON hợp lệ nhưng thiếu message.
            elif model == "mock-invalid-response":
                self.send_json(200, {
                    "model": model,
                    "done": True,
                    "done_reason": "stop"
                })

            # Mode mặc định: Response thành công.
            else:
                response_data = {
                    "model": model or "mock-model",
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

                self.send_json(200, response_data)

            print("[Mock Server] Response sent successfully.")

        except json.JSONDecodeError:
            self.send_json(400, {
                "error": "Invalid JSON"
            })

        except BrokenPipeError:
            print("[Mock Server] Client disconnected.")

    def log_message(self, format, *args):
        print(f"[HTTP] {format % args}")


def main():
    server = HTTPServer((HOST, PORT), MockOllamaHandler)

    print(f"[Mock Server] Running at http://{HOST}:{PORT}")
    print("[Mock Server] Waiting for requests...")

    try:
        server.serve_forever()

    except KeyboardInterrupt:
        print("\n[Mock Server] Shutting down...")

    finally:
        server.server_close()


if __name__ == "__main__":
    main()
