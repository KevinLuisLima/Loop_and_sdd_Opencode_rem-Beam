#!/usr/bin/env python3
import os
import subprocess
import sys

ENGINE = os.environ.get("CHESS_ENGINE", "./build/chess")
WEIGHTS = os.environ.get("CHESS_WEIGHTS", "models/model_0.bin")

def get_fen():
    if len(sys.argv) > 1:
        return " ".join(sys.argv[1:])
    data = sys.stdin.read().strip()
    return data

def main():
    fen = get_fen()
    if not fen:
        print("e2e4")
        return
    try:
        out = subprocess.check_output([ENGINE, "--fen", fen, "--weights", WEIGHTS, "--movetime", "5000", "--depth", "8"], stderr=subprocess.DEVNULL, timeout=9).decode().strip()
        move = out.split()[0]
        if not move:
            move = "e2e4"
        print(move)
    except Exception:
        print("e2e4")

if __name__ == "__main__":
    main()
