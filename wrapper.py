#!/usr/bin/env python3
import subprocess, sys, os

ENGINE = os.path.join(os.path.dirname(__file__), "engine")

def main():
    if len(sys.argv) >= 2:
        fen = sys.argv[1]
    else:
        fen = sys.stdin.read().strip()
    if not fen:
        fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    time_ms = 7000
    if len(sys.argv) >= 3:
        try:
            time_ms = int(sys.argv[2])
        except ValueError:
            pass
    weights = "weights.bin"
    if len(sys.argv) >= 4:
        weights = sys.argv[3]
    try:
        res = subprocess.run([ENGINE, fen, weights, str(time_ms)],
                             capture_output=True, text=True, timeout=15)
        move = res.stdout.strip()
        if res.returncode == 0 and move:
            print(move)
            return
    except Exception:
        pass
    # contingency: try engine on startpos? Just print a legal-ish fallback computed by engine from a fresh start is not valid;
    # Instead return error code with message, as allowed.
    sys.stderr.write("engine failed to produce move\n")
    sys.exit(1)

if __name__ == "__main__":
    main()
