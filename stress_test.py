#!/usr/bin/env python3
"""
stress_test.py - concurrency stress test for the MSMS server.

Spins up several simulated clients in parallel, each opening its own
connection to msms_server over the Unix socket and hammering it with
logins, inventory adds, supply records, and distribution requests -
the exact mix of operations that would surface a locking bug (lost
updates, races, deadlocks) if one existed.

Requires: msms_server already running (make run-server, in another
terminal). Requires only the Python 3 standard library - no pip
installs needed.

Usage:
    python3 stress_test.py                 # 15 clients, 30 ops each
    python3 stress_test.py -c 30 -o 50      # 30 clients, 50 ops each
    python3 stress_test.py -s /tmp/msms.sock
"""

import argparse
import socket
import threading
import time
import random


def client_worker(idx, sock_path, ops, results):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        s.connect(sock_path)
    except OSError as e:
        results.append(f"CONNECT_FAILED: {e}")
        return

    def send(line):
        s.sendall((line + "\n").encode())

    def recv_idle(timeout=0.2):
        s.settimeout(timeout)
        data = b""
        try:
            while True:
                chunk = s.recv(4096)
                if not chunk:
                    break
                data += chunk
        except socket.timeout:
            pass
        return data.decode(errors="replace")

    try:
        recv_idle()
        send("admin")
        send("admin123")
        recv_idle()

        for _ in range(ops):
            choice = random.choice([1, 2, 6, 7, 10])
            if choice == 6:
                send("6")
                send("StressMed")
                send("1")
            elif choice == 7:
                send("7")
                send("StressMed")
                send(f"BATCH{idx}-{random.randint(0, 99999)}")
                send(str(random.randint(1, 100)))
                send("1-1-2030")
                send("5")
            elif choice == 10:
                send("10")
                send("StressMed")
                send(f"BATCH{idx}")
                send(str(random.randint(1, 50)))
                send("1-1-2030")
                send("5")
                send(f"Supplier{idx}")
            else:
                send(str(choice))
            out = recv_idle(0.05)
            if "error" in out.lower() or "corrupt" in out.lower():
                results.append(f"UNEXPECTED_OUTPUT: {out[:200]}")

        send("0")
        send("0")
        recv_idle()
        results.append("OK")
    except Exception as e:
        results.append(f"EXCEPTION: {e}")
    finally:
        s.close()


def main():
    ap = argparse.ArgumentParser(description="Stress-test the MSMS server for concurrency bugs.")
    ap.add_argument("-s", "--socket", default="/tmp/msms.sock", help="server socket path")
    ap.add_argument("-c", "--clients", type=int, default=15, help="number of concurrent simulated clients")
    ap.add_argument("-o", "--ops", type=int, default=30, help="menu operations per client")
    args = ap.parse_args()

    print(f"Connecting {args.clients} concurrent clients to {args.socket}, "
          f"{args.ops} ops each...")

    results = [[] for _ in range(args.clients)]
    threads = [
        threading.Thread(target=client_worker, args=(i, args.socket, args.ops, results[i]))
        for i in range(args.clients)
    ]

    start = time.time()
    for t in threads:
        t.start()
    for t in threads:
        t.join(timeout=60)
    elapsed = time.time() - start

    failed = 0
    for i, r in enumerate(results):
        bad = [x for x in r if not x.startswith("OK")]
        if bad:
            failed += 1
            print(f"  client {i}: {bad}")

    print(f"\nDone in {elapsed:.2f}s - {args.clients - failed}/{args.clients} clients finished cleanly.")
    if failed:
        print("Some clients hit errors above - check the server's stdout/stderr too.")
    else:
        print("No errors or unexpected output detected.")
        print("Check data/inventory.dat and data/audit.log by hand if you want to")
        print("verify totals add up (e.g. every 'stock increase' logged should be")
        print("reflected in the final quantity, with nothing silently dropped).")


if __name__ == "__main__":
    main()