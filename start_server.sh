#!/bin/bash
echo "Starting AeroFirmware Ground Station Server..."
echo

# Activate virtual environment
source .venv/Scripts/activate

# Start server
python server.py --serial COM3 --baud 921600
