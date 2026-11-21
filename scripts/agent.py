#!/usr/bin/env python3
import os, sys, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CHESS = os.path.join(ROOT, "chess")
WEIGHTS = os.path.join(ROOT, "weights", "default.bin")

def main():
    fen = None
    if len(sys.argv) > 1:
        fen = " ".join(sys.argv[1:])
    else:
        fen = sys.stdin.read().strip()
    if not fen:
        fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    try:
        r = subprocess.run([CHESS, "--fen", fen, "--weights", WEIGHTS, "--time", "7000", "--depth", "8"],
                           capture_output=True, text=True, timeout=9.5)
    except subprocess.TimeoutExpired:
        sys.stderr.write("timeout\n")
        sys.exit(1)
    if r.returncode != 0 or not r.stdout.strip():
        sys.stderr.write(r.stderr.strip() + "\n")
        sys.exit(1)
    print(r.stdout.strip().split()[0])

if __name__ == "__main__":
    main()
