import argparse, os, subprocess, sys

DEFAULT_WEIGHTS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "models", "model_base.wts")
DEFAULT_BINARY = os.path.join(os.path.dirname(os.path.abspath(__file__)), "build", "engine")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("fen", nargs="?", default=None)
    ap.add_argument("--fen", dest="fen_opt", default=None)
    ap.add_argument("--weights", default=DEFAULT_WEIGHTS)
    ap.add_argument("--binary", default=DEFAULT_BINARY)
    ap.add_argument("--time", type=int, default=7000)
    args = ap.parse_args()

    fen = args.fen_opt or args.fen
    if not fen:
        data = sys.stdin.read().strip()
        # accept "position fen ..." or raw FEN
        if data.lower().startswith("position fen "):
            fen = data[len("position fen "):].strip()
        else:
            fen = data
    if not fen:
        fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

    try:
        out = subprocess.run([args.binary, "--fen", fen, "--weights", args.weights,
                              "--time", str(args.time), "--depth", "64"],
                             capture_output=True, text=True, timeout=args.time/1000 + 10)
    except Exception:
        print("e2e4")
        return

    move = None
    for line in out.stdout.splitlines():
        toks = line.split()
        if len(toks) >= 2 and toks[0] == "bestmove":
            move = toks[1]
            break
    if not move:
        # contingency: legal move from engine? fallback to first legal by scanning? Use simple default
        move = "e2e4"
    print(move)

if __name__ == "__main__":
    main()
