@echo off
echo Starting AeroFirmware Ground Station Server...
echo.

python server.py --serial COM3 --baud 921600 --port 8001

pause
