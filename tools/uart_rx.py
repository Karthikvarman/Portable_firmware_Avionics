import argparse
import csv
import os
import sys
import time
from datetime import datetime
import serial
RED    = "\033[91m"
GREEN  = "\033[92m"
YELLOW = "\033[93m"
CYAN   = "\033[96m"
RESET  = "\033[0m"
BOLD   = "\033[1m"
CLEAR  = "\033[2J\033[H"
def xor_checksum(s: str) -> int:
    csum = 0
    for c in s:
        csum ^= ord(c)
    return csum
def validate_frame(line: str) -> tuple[bool, list]:
    line = line.strip()
    if not line.startswith("$") or "*" not in line:
        return False, []
    star   = line.rfind("*")
    body   = line[1:star]           
    csum_s = line[star+1:star+3]
    try:
        csum_r = int(csum_s, 16)
    except ValueError:
        return False, []
    csum_c = xor_checksum(body)
    if csum_r != csum_c:
        return False, []
    parts = body.split(",")
    if parts[0] != "AERO" or len(parts) < 10:
        return False, []
    return True, parts[1:]
def parse_fields(fields: list) -> dict | None:
    try:
        return {
            "tick_ms":  int(fields[0]),
            "roll":     float(fields[1]),
            "pitch":    float(fields[2]),
            "yaw":      float(fields[3]),
            "alt":      float(fields[4]),
            "lat":      float(fields[5]),
            "lon":      float(fields[6]),
            "sats":     int(fields[7]),
            "co_ppm":   float(fields[8]),
        }
    except (ValueError, IndexError):
        return None
def print_dashboard(d: dict, stats: dict):
    print(CLEAR, end="")
    t = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    print(f"{BOLD}{CYAN}╔══════════════════════════════════════════════════════╗{RESET}")
    print(f"{BOLD}{CYAN}║      AeroFirmware Ground Station — Live Monitor      ║{RESET}")
    print(f"{BOLD}{CYAN}╚══════════════════════════════════════════════════════╝{RESET}")
    print(f"  Time: {t}  |  Tick: {CYAN}{d['tick_ms']:>10} ms{RESET}  |  "
          f"Rate: {GREEN}{stats['fps']:>4.1f} Hz{RESET}  |  "
          f"Errors: {RED}{stats['errors']}{RESET}")
    print()
    print(f"{BOLD}  ─── Attitude (EKF) ───────────────────────────────────{RESET}")
    roll_bar  = bar(d['roll'],  -180, 180)
    pitch_bar = bar(d['pitch'],  -90,  90)
    yaw_bar   = bar(d['yaw'],     0, 360)
    print(f"  Roll  : {YELLOW}{d['roll']:>8.2f}°{RESET}  {roll_bar}")
    print(f"  Pitch : {YELLOW}{d['pitch']:>8.2f}°{RESET}  {pitch_bar}")
    print(f"  Yaw   : {YELLOW}{d['yaw']:>8.2f}°{RESET}  {yaw_bar}")
    print()
    print(f"{BOLD}  ─── Navigation ────────────────────────────────────────{RESET}")
    print(f"  Altitude: {CYAN}{d['alt']:>9.2f} m{RESET}")
    print(f"  Latitude: {CYAN}{d['lat']:>12.6f}°{RESET}  "
          f"Longitude: {CYAN}{d['lon']:>12.6f}°{RESET}")
    sats_color = GREEN if d['sats'] >= 4 else RED
    print(f"  Satellites: {sats_color}{d['sats']}{RESET} "
          f"({'3D FIX' if d['sats'] >= 4 else 'NO FIX'})")
    print()
    print(f"{BOLD}  ─── Air Quality ────────────────────────────────────────{RESET}")
    co = d['co_ppm']
    co_color = GREEN if co < 9 else (YELLOW if co < 35 else RED)
    print(f"  CO Gas  : {co_color}{co:>8.1f} ppm{RESET}  "
          f"{'SAFE' if co < 9 else 'CAUTION' if co < 35 else '!! DANGER !!'}")
    print()
    print(f"{BOLD}  ─── Statistics ─────────────────────────────────────────{RESET}")
    print(f"  Frames: {stats['frames']:>8}  |  Valid: {GREEN}{stats['valid']}{RESET}  "
          f"  |  CSV: {stats.get('csv_path','none')}")
    print()
def bar(val: float, lo: float, hi: float, width: int = 30) -> str:
    pct = min(1.0, max(0.0, (val - lo) / (hi - lo)))
    filled = int(pct * width)
    return f"[{GREEN}{'█'*filled}{RESET}{'░'*(width-filled)}]"
def main():
    parser = argparse.ArgumentParser(description="AeroFirmware UART Receiver")
    parser.add_argument("--port",  default="COM3",   help="Serial port (COM3 / /dev/ttyUSB0)")
    parser.add_argument("--baud",  default=921600, type=int, help="Baud rate (default: 921600)")
    parser.add_argument("--csv",   default="aero_log.csv", help="CSV log file path")
    parser.add_argument("--plain", action="store_true", help="Plain text output (no ANSI)")
    args = parser.parse_args()
    csv_path = args.csv
    stats = {"frames": 0, "valid": 0, "errors": 0, "fps": 0.0, "csv_path": csv_path}
    last_fps_time = time.time()
    fps_frame_count = 0
    print(f"Opening {args.port} @ {args.baud} baud...")
    try:
        ser = serial.Serial(args.port, args.baud, timeout=2)
    except serial.SerialException as e:
        print(f"[ERROR] {e}")
        sys.exit(1)
    print(f"Connected. Logging to {csv_path}. Press Ctrl+C to stop.\n")
    csv_file   = open(csv_path, "w", newline="", buffering=1)
    csv_writer = csv.DictWriter(csv_file, fieldnames=[
        "timestamp", "tick_ms", "roll", "pitch", "yaw",
        "alt", "lat", "lon", "sats", "co_ppm"])
    csv_writer.writeheader()
    buf = b""
    last_data: dict | None = None
    try:
        while True:
            chunk = ser.read(256)
            if chunk:
                buf += chunk
                while b"\n" in buf:
                    line_b, buf = buf.split(b"\n", 1)
                    line = line_b.decode("ascii", errors="replace")
                    stats["frames"] += 1
                    valid, fields = validate_frame(line)
                    if valid:
                        d = parse_fields(fields)
                        if d:
                            d["timestamp"] = datetime.now().isoformat()
                            csv_writer.writerow(d)
                            stats["valid"] += 1
                            fps_frame_count += 1
                            last_data = d
                        else:
                            stats["errors"] += 1
                    else:
                        stats["errors"] += 1
                    now = time.time()
                    if now - last_fps_time >= 1.0:
                        stats["fps"] = fps_frame_count / (now - last_fps_time)
                        fps_frame_count = 0
                        last_fps_time = now
                    if last_data and not args.plain:
                        print_dashboard(last_data, stats)
                    elif last_data and args.plain:
                        d = last_data
                        print(f"[{d['tick_ms']:08d}] R:{d['roll']:+7.2f}° "
                              f"P:{d['pitch']:+7.2f}° Y:{d['yaw']:+7.2f}° "
                              f"Alt:{d['alt']:7.1f}m CO:{d['co_ppm']:5.1f}ppm "
                              f"Sats:{d['sats']}")
    except KeyboardInterrupt:
        print(f"\n\nStopped. {stats['valid']} valid frames logged to {csv_path}")
    finally:
        csv_file.close()
        ser.close()
if __name__ == "__main__":
    main()