#!/usr/bin/env python3
import argparse, os, subprocess, sys

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fen", default=None)
    ap.add_argument("positional", nargs="*")
    ap.add_argument("--engine", default=None)
    ap.add_argument("--weights", default=None)
    ap.add_argument("--time", type=int, default=7000)
    args = ap.parse_args()

    fen = args.fen
    if fen is None and args.positional:
        fen = args.positional[0]
    if fen is None:
        fen = sys.stdin.readline().strip()

    base = os.path.dirname(os.path.abspath(__file__))
    project = os.path.dirname(base)
    engine = args.engine or os.path.join(project, "build", "chess")
    weights = args.weights or os.path.join(project, "models", "model_0.bin")

    if not os.path.exists(engine):
        # fallback bin name
        engine = os.path.join(project, "build", "chess")
    if not os.path.exists(weights):
        print("0000")
        return 0

    cmd = [engine, "--fen", fen, "--weights", weights, "--time", str(int(args.time*0.7)), "--json"]
    try:
        out = subprocess.run(cmd, capture_output=True, text=True, timeout=max(1, args.time/1000+2))
    except Exception as e:
        print("0000")
        return 0
    if out.returncode != 0:
        print("0000")
        return 0
    try:
        import json
        d = json.loads(out.stdout.strip())
        mv = d.get("move", "0000")
        print(mv)
        return 0
    except Exception:
        txt = out.stdout.strip().split()
        print(txt[0] if txt else "0000")
        return 0

if __name__ == "__main__":
    main()
