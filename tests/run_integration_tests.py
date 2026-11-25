import os
import subprocess
import sys
import threading

from mock_server import create_server


def main():
    if len(sys.argv) != 2:
        print("Usage: run_integration_tests.py <integration-test-executable>")
        return 2

    server = create_server(host="127.0.0.1", port=0)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()

    try:
        host, port = server.server_address[:2]
        environment = os.environ.copy()
        environment["OGK_TEST_BASE_URL"] = f"http://{host}:{port}"

        result = subprocess.run(
            [sys.argv[1]],
            env=environment,
            check=False
        )
        return result.returncode
    finally:
        server.shutdown()
        server.server_close()
        thread.join()


if __name__ == "__main__":
    sys.exit(main())
