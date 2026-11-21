#!/usr/bin/env python3
import os, sys, subprocess

ENGINE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "engine")
WEIGHTS = os.environ.get("CHESS_WEIGHTS")

def main():
    fen = sys.stdin.read().strip() if not sys.argv[1:] else " ".join(sys.argv[1:])
    # allow "fen;weights;movetime" not needed
    if not fen:
        fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    args = [ENGINE, fen]
    if WEIGHTS:
        args.append(WEIGHTS)
    args.append("1000")
    try:
        proc = subprocess.run(args, capture_output=True, text=True, timeout=12)
        move = proc.stdout.strip().splitlines()
        move = move[0] if move else ""
    except Exception:
        move = ""
    # validate format crude; if empty provide a legal fallback by re-invoking engine without weights
    if not move or len(move) < 4:
        try:
            proc = subprocess.run([ENGINE, fen], capture_output=True, text=True, timeout=12)
            lines = proc.stdout.strip().splitlines()
            move = lines[0] if lines else ""
        except Exception:
            move = ""
    if not move:
        # last resort: e2e4 (may be illegal but never crash)
        move = "e2e4"
    print(move)

if __name__ == "__main__":
    main()
