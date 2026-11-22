#!/usr/bin/env python3
import os
import subprocess
import sys

def bin_path():
    env = os.environ.get("CHESS_BIN")
    if env:
        return env
    here = os.path.dirname(os.path.abspath(__file__))
    cand = [
        os.path.join(here, "..", "build", "chess"),
        "build/chess",
        "./build/chess",
        os.path.join(here, "..", "build_cmake", "chess"),
    ]
    for c in cand:
        if os.path.isfile(c):
            return c
    return "build/chess"

def main():
    if len(sys.argv) == 1:
        fen = sys.stdin.read().strip()
    else:
        fen = " ".join(sys.argv[1:]).strip()
    if not fen:
        print("usage: wrapper.py <FEN>", file=sys.stderr)
        return 2
    try:
        r = subprocess.run(
            [bin_path(), fen],
            capture_output=True, text=True, timeout=12,
        )
    except Exception as e:
        print(f"erro tratado: {e}", file=sys.stderr)
        return 1
    if r.returncode != 0:
        sys.stderr.write(r.stderr if r.stderr else "erro tratado\n")
        return r.returncode
    out = r.stdout.strip()
    print(out if out else "0000")
    return 0

if __name__ == "__main__":
    main()