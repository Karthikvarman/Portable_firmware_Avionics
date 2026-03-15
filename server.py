import sys
import os
from pathlib import Path

if not (hasattr(sys, 'real_prefix') or (hasattr(sys, 'base_prefix') and sys.base_prefix != sys.prefix)):
    base_dir = Path(__file__).parent
    venv_python = base_dir / "venv" / "Scripts" / "python.exe"
    if venv_python.exists():
        print(f"[INFO] Using virtual environment: {venv_python}")
        import subprocess
        subprocess.run([str(venv_python), str(Path(__file__).absolute())] + sys.argv[1:])
        sys.exit(0)
import argparse
import asyncio
import json
import time
import struct
import threading
import subprocess
import shutil
import socket
from typing import Optional, Set
import serial
import serial.tools.list_ports
import uvicorn
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, UploadFile, File, HTTPException, Body, Form, Request
from fastapi.responses import HTMLResponse, FileResponse, JSONResponse
from fastapi.staticfiles import StaticFiles
from fastapi.middleware.cors import CORSMiddleware
from contextlib import asynccontextmanager

def get_local_ip():
    """Get the local IP address of the machine."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"

APP_DIR  = Path(__file__).parent / "ui"
FW_DIR   = Path(__file__).parent / "firmware_uploads"
FW_DIR.mkdir(exist_ok=True)

@asynccontextmanager
async def lifespan(app: FastAPI):
    global loop
    loop = asyncio.get_running_loop()
    local_ip = get_local_ip()
    print("=" * 60)
    print("  AeroFirmware Ground Station Server v1.0.0")
    print(f"  Local:   http://localhost:8001/")
    print(f"  Network: http://{local_ip}:8001/  (Use this for phone/tablet)")
    print(f"  API:     http://localhost:8001/api/ports")
    print(f"  WS:      ws://localhost:8001/ws/telemetry")
    print("=" * 60)
    yield
    print("[INFO] Shutting down gracefully...")
    if sm.connected:
        sm.disconnect()
app = FastAPI(title="AeroFirmware Ground Station", version="1.0.0", lifespan=lifespan)
app.add_middleware(CORSMiddleware,
    allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])
if APP_DIR.exists():
    app.mount("/ui", StaticFiles(directory=str(APP_DIR)), name="ui")
class SerialManager:
    def __init__(self):
        self.ser: Optional[serial.Serial] = None
        self.port_name: str = ""
        self.baud: int = 921600
        self.lock = threading.Lock()
        self.connected = False
        self.clients: Set[WebSocket] = set()
        self._rx_thread: Optional[threading.Thread] = None
        self._running = False
    def list_ports(self):
        ports = serial.tools.list_ports.comports()
        return [{"port": p.device, "desc": p.description, "hwid": p.hwid}
                for p in sorted(ports)]
    def connect(self, port: str, baud: int) -> bool:
        with self.lock:
            try:
                self.ser = serial.Serial(port=port, baudrate=baud,
                                         timeout=1, write_timeout=2)
                self.port_name  = port
                self.baud       = baud
                self.connected  = True
                self._running   = True
                self._rx_thread = threading.Thread(target=self._rx_loop, daemon=True)
                self._rx_thread.start()
                print(f"[serial] Connected: {port} @ {baud} baud")
                return True
            except serial.SerialException as e:
                print(f"[serial] Error: {e}")
                return False
    def disconnect(self):
        self._running = False
        with self.lock:
            if self.ser and self.ser.is_open:
                self.ser.close()
            self.connected = False
        print("[serial] Disconnected")
    def _rx_loop(self):
        buf = b""
        while self._running and self.ser and self.ser.is_open:
            try:
                chunk = self.ser.read(256)
                if chunk:
                    buf += chunk
                    while b"\n" in buf:
                        line, buf = buf.split(b"\n", 1)
                        line = line.strip()
                        if line.startswith(b"$AERO"):
                            msg = line.decode("ascii", errors="replace")
                            asyncio.run_coroutine_threadsafe(
                                self._broadcast(msg), loop)
            except Exception:
                break
    async def _broadcast(self, msg: str):
        dead = set()
        for ws in self.clients:
            try:
                await ws.send_text(msg)
            except Exception:
                dead.add(ws)
        self.clients -= dead
    def write(self, data: bytes):
        with self.lock:
            if self.ser and self.ser.is_open:
                self.ser.write(data)
sm = SerialManager()
loop: asyncio.AbstractEventLoop = None  
@app.get("/", response_class=HTMLResponse)
async def serve_index():
    index = APP_DIR / "index.html"
    if index.exists():
        return index.read_text(encoding="utf-8")
    return HTMLResponse("<h1>AeroFirmware Ground Station</h1><p>UI not found.</p>")
@app.get("/style.css")
async def serve_css():
    return FileResponse(str(APP_DIR / "style.css"), media_type="text/css")
@app.get("/app.js")
async def serve_js():
    return FileResponse(str(APP_DIR / "app.js"), media_type="application/javascript")
@app.get("/api/ports")
async def list_ports():
    return {"ports": sm.list_ports()}
@app.post("/api/command")
async def send_command(cmd: str = Body(..., embed=True)):
    if not sm.connected:
        raise HTTPException(400, "Device not connected")
    c = cmd.strip() + "\n"
    try:
        sm.write(c.encode("ascii"))
        return {"success": True, "msg": f"Command sent: {c.strip()}"}
    except Exception as e:
        raise HTTPException(500, f"Failed to send command: {str(e)}")
@app.get("/api/status")
async def get_status():
    return {
        "connected":  sm.connected,
        "port":       sm.port_name,
        "baud":       sm.baud,
        "clients":    len(sm.clients),
        "server_time": int(time.time() * 1000),
    }
@app.post("/api/connect")
async def connect(body: dict):
    port = body.get("port", "")
    baud = int(body.get("baud", 921600))
    if not port:
        raise HTTPException(400, "port is required")
    if sm.connected:
        sm.disconnect()
    ok = sm.connect(port, baud)
    if not ok:
        raise HTTPException(500, f"Failed to open {port}")
    return {"success": True, "port": port, "baud": baud}
@app.post("/api/disconnect")
async def disconnect():
    sm.disconnect()
    return {"success": True}

@app.post("/api/flash")
async def flash_firmware(
    file: UploadFile = File(...),
    mcu: str = "STM32H7",
    port: str = Form(None),
    erase: bool = Form(True),
    verify: bool = Form(True),
    reset: bool = Form(True),
):
    was_connected = sm.connected
    orig_port = sm.port_name
    orig_baud = sm.baud
    
    try:
        detected_mcu = mcu
        filename_upper = file.filename.upper()
        mcu_list = ["STM32H7", "STM32F4", "STM32F7", "STM32L4", "STM32L0", "ESP32", "ESP8266"]
        for m in mcu_list:
            if m in filename_upper:
                detected_mcu = m
                break
        
        if "DEVKITV1" in filename_upper: detected_mcu = "ESP32_DEVKITV1"
        if "DEV_MODULE" in filename_upper: detected_mcu = "ESP32_DEV_MODULE"

        fw_path = FW_DIR / file.filename
        log_lines = []
        log_lines.append(f"[AeroFlash] Firmware path: {fw_path}")
        try:
            fw_path.write_bytes(await file.read())
            size_kb = fw_path.stat().st_size / 1024
            log_lines.append(f"[AeroFlash] File written successfully: {size_kb:.1f} KB")
        except Exception as e:
            log_lines.append(f"[ERR] Failed to write firmware file: {str(e)}")
            return {"success": False, "log": log_lines}

        log_lines.append(f"[AeroFlash] File: {file.filename} ({size_kb:.1f} KB)")
        log_lines.append(f"[AeroFlash] Target MCU: {detected_mcu} (Detected from file: {detected_mcu != mcu})")

        if detected_mcu.startswith("STM32"):
            cmd = ["STM32_Programmer_CLI", "-c", f"port={port if port else 'SWD'}", "-w", str(fw_path)]
            if erase:  cmd += ["-e", "all"]
            if verify: cmd += ["-v"]
            if reset:  cmd += ["-rst"]
        elif "ESP" in detected_mcu:
            chip = detected_mcu.lower().replace("-", "").replace("_", "")
            if "devkit" in chip or "devmodule" in chip: chip = "esp32"
            
            cmd = ["esptool.py"]
            if port: cmd += ["--port", port]
            cmd += ["--baud", "921600", "--chip", chip]
            
            log_lines.append(f"[AeroFlash] Detecting esptool...")
            
            if not shutil.which(cmd[0]):
                log_lines.append(f"[AeroFlash] {cmd[0]} not found in PATH")
                if shutil.which("esptool"):
                    cmd[0] = "esptool"
                    log_lines.append(f"[AeroFlash] Using 'esptool' command")
                else:
                    cmd = [sys.executable, "-m", "esptool"] + cmd[1:]
                    log_lines.append(f"[AeroFlash] Using 'python -m esptool' module")
            else:
                log_lines.append(f"[AeroFlash] Using '{cmd[0]}' from PATH")
            
            if erase:
                erase_cmd = cmd + ["erase-flash"]
                log_lines.append(f"[AeroFlash] Erasing flash...")
                try:
                    result = subprocess.run(erase_cmd, capture_output=True, text=True, timeout=60)
                    if result.returncode != 0:
                        log_lines += result.stderr.splitlines() if result.stderr else []
                        log_lines.append("[ERR] Flash erase failed")
                        return {"success": False, "log": log_lines}
                    else:
                        log_lines.append("[AeroFlash] Flash erase completed")
                except Exception as e:
                    log_lines.append(f"[ERR] Erase command failed: {str(e)}")
                    return {"success": False, "log": log_lines}
            
            cmd += ["write_flash", "--flash-mode", "dio", "--flash-freq", "40m", "--flash-size", "4MB", "0x1000", str(fw_path)]
        else:
            return {"success": False, "log": [f"[ERR] Unsupported MCU: {detected_mcu}"]}
        
        print(f"[AeroFlash] Executing: {' '.join(cmd)}")
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
            
            if result.stdout: 
                print(result.stdout)
                log_lines += result.stdout.splitlines()
            if result.stderr: 
                print(result.stderr)
                log_lines += result.stderr.splitlines()

            if was_connected:
                try:
                    print(f"[AeroFlash] Re-acquiring serial port {orig_port}...")
                    sm.connect(orig_port, orig_baud)
                    log_lines.append(f"[AeroFlash] Re-acquired serial port {orig_port} ✓")
                except Exception as re_err:
                    log_lines.append(f"[WARN] Failed to re-acquire serial port: {str(re_err)}")

            return {"success": True, "log": log_lines}
        except subprocess.TimeoutExpired:
            if was_connected:
                try: sm.connect(orig_port, orig_baud)
                except: pass
            log_lines.append("[ERR] Flash operation timed out")
            return {"success": False, "log": log_lines}
        except FileNotFoundError:
            if was_connected:
                try: sm.connect(orig_port, orig_baud)
                except: pass
            log_lines.append("[ERR] Flash programmer not found")
            return {"success": False, "log": log_lines}
        except Exception as e:
            if was_connected:
                try: sm.connect(orig_port, orig_baud)
                except: pass
            log_lines.append(f"[ERR] {str(e)}")
            return {"success": False, "log": log_lines}
    finally:
        if was_connected:
            try: sm.connect(orig_port, orig_baud)
            except: pass
@app.websocket("/ws/telemetry")
async def telemetry_ws(ws: WebSocket):
    await ws.accept()
    sm.clients.add(ws)
    try:
        while True:
            msg = await asyncio.wait_for(ws.receive_text(), timeout=30.0)
            if msg == "ping":
                await ws.send_text("pong")
    except (WebSocketDisconnect, asyncio.TimeoutError):
        pass
    finally:
        sm.clients.discard(ws)
@app.exception_handler(Exception)
async def global_exception_handler(request: Request, exc: Exception):
    return JSONResponse(
        status_code=500,
        content={"success": False, "log": [f"[ERR] Internal server error: {str(exc)}"]}
    )
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="AeroFirmware Ground Station Server")
    parser.add_argument("--host",   default="0.0.0.0",  help="Bind host")
    parser.add_argument("--port",   default=8001, type=int, help="HTTP port")
    parser.add_argument("--serial", default="",   help="Auto-connect serial port")
    parser.add_argument("--baud",   default=921600, type=int, help="Baud rate")
    parser.add_argument("--reload", action="store_true", help="Enable hot reload")
    args = parser.parse_args()
    if args.serial:
        print(f"[INFO] Auto-connecting to {args.serial} @ {args.baud} baud...")
        sm.connect(args.serial, args.baud)
    uvicorn.run("server:app",
                host=args.host,
                port=args.port,
                reload=args.reload,
                log_level="info")