#!/usr/bin/env python3
"""A fake Klipper printer for the Klipper backend, over Moonraker's WebSocket.

Tiger Studio has an HTTP mock (scripts/moonraker-mock.mjs in its repository);
the device talks JSON-RPC over the WebSocket instead, so this is that half.
Python standard library only.

  python3 scripts/moonraker-ws-mock.py                       # plain printer, :7125
  python3 scripts/moonraker-ws-mock.py --profile happyhare   # Happy Hare, 4 gates
  python3 scripts/moonraker-ws-mock.py --profile afc         # AFC, 4 lanes
  python3 scripts/moonraker-ws-mock.py --port 7126 --api-key secret

Then add a Klipper printer in Tiger Studio at <this machine's IP>[:port] and let
the device sync. Studio itself talks HTTP to Moonraker, so the few HTTP
requests its add flow and its card make are answered too (server/printer/
machine info, the object list and query, an empty webcam list). What it answers: server.connection.identify,
printer.objects.list, printer.objects.query and printer.gcode.script with
MMU_GATE_MAP and AFC's SET_MATERIAL / SET_COLOR / SET_WEIGHT parsed into the state - so a
write is read back exactly as it would be from a printer. It also pushes a
notify_proc_stat_update every second, as Moonraker does to every client.

The AFC profile publishes its lanes twice, as `AFC_stepper` and `AFC_lane`,
the way an OpenCentauri CC1 does: the backend must list each lane once.
"""

import argparse
import base64
import hashlib
import json
import socket
import struct
import sys
import threading
import time

GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


def build_state(profile):
    s = {"toolhead": {"extruder": "extruder"}, "extruder": {}, "heater_bed": {},
         "print_stats": {"state": "standby"}, "webhooks": {"state": "ready"}}
    if profile == "happyhare":
        s["mmu"] = {"num_gates": 4, "gate": 1,
                    "gate_status": [1, 1, 0, -1],
                    "gate_material": ["PLA", "PETG", "", "ASA"],
                    "gate_color": ["FF0000", "00FF00", "", "red"]}
    elif profile == "afc":
        lanes = ["lane1", "lane2", "lane3", "lane4"]
        s["AFC"] = {"lanes": lanes, "current_lane": "lane2"}
        for i, lane in enumerate(lanes):
            obj = {"map": f"T{i}", "material": ["PLA", "PETG", "", "TPU"][i],
                   "color": ["#FF8800", "#0055FF", "#000000", "#FFFFFF"][i],
                   "load": i != 2, "prep": i != 2, "tool_loaded": False, "weight": 850,
                   "initial_weight": 1000}
            s[f"AFC_stepper {lane}"] = obj
            s[f"AFC_lane {lane}"] = obj          # the same lane, published twice
    else:
        s["extruder1"] = {}
    return s


def params_of(line):
    out = {}
    for tok in line.split()[1:]:
        if "=" in tok:
            k, v = tok.split("=", 1)
            out[k.upper()] = v
    return out


def run_script(state, script):
    for line in script.split("\n"):
        line = line.strip()
        if not line:
            continue
        cmd, p = line.split()[0].upper(), params_of(line)
        if cmd == "MMU_GATE_MAP" and "mmu" in state:
            g = int(p.get("GATE", -1))
            m = state["mmu"]
            if not 0 <= g < m["num_gates"]:
                raise ValueError(f"Invalid gate {p.get('GATE')}")
            if "MATERIAL" in p:
                m["gate_material"][g] = p["MATERIAL"]
            if "COLOR" in p:
                m["gate_color"][g] = p["COLOR"].lstrip("#")
            m["gate_status"][g] = 1
        elif cmd in ("SET_MATERIAL", "SET_COLOR", "SET_WEIGHT") and "AFC" in state:
            lane = state.get(f"AFC_lane {p.get('LANE', '')}")
            if lane is None:
                raise ValueError(f"Unknown lane {p.get('LANE')}")
            if cmd == "SET_MATERIAL":
                lane["material"] = p.get("MATERIAL", "")
            elif cmd == "SET_WEIGHT":
                lane["weight"] = float(p.get("WEIGHT", 0))
            else:
                lane["color"] = "#" + p.get("COLOR", "").lstrip("#")
        else:
            raise ValueError(f"Unknown command \"{cmd}\"")


def recv_frame(conn):
    head = conn.recv(2)
    if len(head) < 2:
        return None, None
    op, n = head[0] & 0x0F, head[1] & 0x7F
    if n == 126:
        n = struct.unpack(">H", conn.recv(2))[0]
    elif n == 127:
        n = struct.unpack(">Q", conn.recv(8))[0]
    mask = conn.recv(4) if head[1] & 0x80 else b"\0\0\0\0"
    data = b""
    while len(data) < n:
        chunk = conn.recv(n - len(data))
        if not chunk:
            return None, None
        data += chunk
    return op, bytes(b ^ mask[i % 4] for i, b in enumerate(data))


def send_frame(conn, payload, op=1):
    n = len(payload)
    head = bytes([0x80 | op])
    if n < 126:
        head += bytes([n])
    elif n < 65536:
        head += bytes([126]) + struct.pack(">H", n)
    else:
        head += bytes([127]) + struct.pack(">Q", n)
    conn.sendall(head + payload)


def serve_http(conn, addr, req_line, headers, body, state, api_key, lock):
    method, target = (req_line.split(" ") + ["", ""])[:2]
    path, _, query = target.partition("?")
    if api_key and headers.get("x-api-key") != api_key:
        result, code = None, 401
    else:
        code = 200
        with lock:
            if path == "/server/info":
                result = {"klippy_connected": True, "klippy_state": "ready", "components": []}
            elif path == "/printer/info":
                result = {"state": "ready", "state_message": "Printer is ready",
                          "hostname": "moonraker-mock", "software_version": "v0.12.0-mock"}
            elif path == "/machine/system_info":
                result = {"system_info": {"cpu_info": {"serial_number": "MOCK0001"}}}
            elif path == "/printer/objects/list":
                result = {"objects": list(state.keys())}
            elif path == "/server/webcams/list":
                result = {"webcams": []}
            elif path == "/printer/objects/query":
                wanted = {}
                if method == "POST" and body:
                    wanted = (json.loads(body or "{}").get("objects") or {})
                else:
                    wanted = {k: None for k in query.split("&") if k}
                out = {}
                for name, fields in wanted.items():
                    if name in state:
                        obj = state[name]
                        out[name] = {f: obj[f] for f in (fields or obj) if f in obj}
                result = {"eventtime": time.time(), "status": out}
            else:
                result, code = None, 404
    payload = json.dumps({"result": result} if code == 200 else {"error": {"code": code}}).encode()
    reason = {200: "OK", 401: "Unauthorized", 404: "Not Found"}[code]
    conn.sendall(f"HTTP/1.1 {code} {reason}\r\nContent-Type: application/json\r\n"
                 f"Content-Length: {len(payload)}\r\nConnection: close\r\n\r\n".encode() + payload)
    print(f"[{addr[0]}] http {method} {path} -> {code}")
    conn.close()


def serve(conn, addr, state, api_key, lock):
    req = b""
    while b"\r\n\r\n" not in req:
        chunk = conn.recv(4096)
        if not chunk:
            return
        req += chunk
    head, _, body = req.partition(b"\r\n\r\n")
    lines = head.decode(errors="replace").split("\r\n")
    headers = {}
    for line in lines[1:]:
        if ":" in line:
            k, v = line.split(":", 1)
            headers[k.strip().lower()] = v.strip()
    if headers.get("upgrade", "").lower() != "websocket":
        need = int(headers.get("content-length", "0") or 0)
        while len(body) < need:
            chunk = conn.recv(4096)
            if not chunk:
                break
            body += chunk
        serve_http(conn, addr, lines[0], headers, body.decode(errors="replace"), state, api_key, lock)
        return
    if "origin" in headers:
        # Moonraker refuses an origin outside cors_domains; the device must send none.
        conn.sendall(b"HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n")
        print(f"[{addr[0]}] refused: Origin header {headers['origin']!r}")
        return
    accept = base64.b64encode(hashlib.sha1((headers.get("sec-websocket-key", "") + GUID)
                                           .encode()).digest()).decode()
    conn.sendall(("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
                  f"Connection: Upgrade\r\nSec-WebSocket-Accept: {accept}\r\n\r\n").encode())
    print(f"[{addr[0]}] websocket open")
    authed = {"ok": not api_key}
    alive = {"on": True}
    send_lock = threading.Lock()

    def push():
        while alive["on"]:
            time.sleep(1)
            msg = {"jsonrpc": "2.0", "method": "notify_proc_stat_update",
                   "params": [{"cpu_temp": 45.0, "moonraker_stats": {"cpu_usage": 2.1}}]}
            try:
                with send_lock:
                    send_frame(conn, json.dumps(msg).encode())
            except OSError:
                return
    threading.Thread(target=push, daemon=True).start()

    def reply(rid, result=None, error=None):
        msg = {"jsonrpc": "2.0", "id": rid}
        if error:
            msg["error"] = {"code": 400, "message": error}
        else:
            msg["result"] = result
        with send_lock:
            send_frame(conn, json.dumps(msg).encode())

    try:
        while True:
            op, data = recv_frame(conn)
            if op is None or op == 8:
                break
            if op == 9:
                with send_lock:
                    send_frame(conn, data, op=10)
                continue
            if op != 1:
                continue
            msg = json.loads(data)
            method, rid, p = msg.get("method"), msg.get("id"), msg.get("params") or {}
            shown = {k: ("***" if k == "api_key" else v) for k, v in p.items()}
            print(f"[{addr[0]}] <- {method} {json.dumps(shown)[:160]}")
            if method == "server.connection.identify":
                if api_key and p.get("api_key") != api_key:
                    reply(rid, error="Unauthorized")
                else:
                    authed["ok"] = True
                    reply(rid, {"connection_id": 1})
                continue
            if not authed["ok"]:
                reply(rid, error="Unauthorized")
                continue
            with lock:
                if method == "printer.objects.list":
                    reply(rid, {"objects": list(state.keys())})
                elif method == "printer.objects.query":
                    out = {}
                    for name, fields in (p.get("objects") or {}).items():
                        if name in state:
                            obj = state[name]
                            out[name] = {f: obj[f] for f in (fields or obj) if f in obj}
                    reply(rid, {"eventtime": time.time(), "status": out})
                elif method == "printer.gcode.script":
                    try:
                        run_script(state, p.get("script", ""))
                        reply(rid, "ok")
                    except ValueError as e:
                        reply(rid, error=str(e))
                else:
                    reply(rid, error=f"Method not found: {method}")
    except (OSError, ValueError) as e:
        print(f"[{addr[0]}] closed: {e}")
    finally:
        alive["on"] = False
        conn.close()
        print(f"[{addr[0]}] websocket closed")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--profile", choices=["plain", "happyhare", "afc"], default="plain")
    ap.add_argument("--port", type=int, default=7125)
    ap.add_argument("--api-key", default="")
    a = ap.parse_args()
    sys.stdout.reconfigure(line_buffering=True)      # each line as it happens, even into a file
    state, lock = build_state(a.profile), threading.Lock()
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", a.port))
    srv.listen(8)
    print(f"Moonraker WebSocket mock, profile {a.profile}, port {a.port}"
          f"{', API key required' if a.api_key else ''}")
    while True:
        conn, addr = srv.accept()
        threading.Thread(target=serve, args=(conn, addr, state, a.api_key, lock), daemon=True).start()


if __name__ == "__main__":
    main()
