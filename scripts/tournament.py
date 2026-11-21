#!/usr/bin/env python3
import argparse, glob, os, random, re, struct, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ARENA = os.path.join(ROOT, "arena")
CHESS = os.path.join(ROOT, "chess")

POSITIONS = [
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "rnbqkb1r/pppp1ppp/5n2/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 4 4",
    "rnbqkbnr/ppp2ppp/3p4/4p3/4P3/3P4/PPP2PPP/RNBQKBNR w KQkq - 0 3",
]

def parse_arena(text):
    d = {}
    for line in text.splitlines():
        for key in ("games", "scoreA", "scoreB", "movesA", "movesB", "suboptA", "suboptB", "subopt_rateA", "subopt_rateB"):
            m = re.search(r"\b" + key + r"=([^\s]+)", line)
            if m:
                d[key] = m.group(1)
    # sum games/moves across lines? This format one summary at end
    return d

def read_weights(path):
    data = open(path, "rb").read()
    ver, n, dtype = struct.unpack_from("<III", data, 8)
    vals = list(struct.unpack_from("<%df" % n, data, 20))
    return ver, n, dtype, vals

def write_weights(path, ver, n, dtype, vals):
    with open(path, "wb") as f:
        f.write(struct.pack("<8s", b"CHESSW1\0"))
        f.write(struct.pack("<III", ver, n, dtype))
        for v in vals:
            f.write(struct.pack("<f", float(v)))

def run_match(a, b, games, depth, time_ms, judge_depth):
    totals = {"games": 0, "scoreA": 0.0, "scoreB": 0.0, "movesA": 0, "movesB": 0, "suboptA": 0, "suboptB": 0}
    for fen in POSITIONS:
        g_per_pos = max(1, games // len(POSITIONS))
        cmd = [ARENA, a, b, str(g_per_pos), str(depth), str(time_ms), fen, str(judge_depth)]
        out = subprocess.run(cmd, capture_output=True, text=True)
        if out.returncode != 0:
            print("ARENA ERROR:", out.stderr, file=sys.stderr)
            continue
        d = parse_arena(out.stdout)
        totals["games"] += int(d.get("games", g_per_pos))
        totals["scoreA"] += float(d.get("scoreA", 0)) * int(d.get("games", g_per_pos))
        totals["scoreB"] += float(d.get("scoreB", 0)) * int(d.get("games", g_per_pos))
        totals["movesA"] += int(d.get("movesA", 0))
        totals["movesB"] += int(d.get("movesB", 0))
        totals["suboptA"] += int(d.get("suboptA", 0))
        totals["suboptB"] += int(d.get("suboptB", 0))
    return totals

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--models-dir", default="weights")
    ap.add_argument("--generations", type=int, default=5)
    ap.add_argument("--games", type=int, default=2)
    ap.add_argument("--depth", type=int, default=3)
    ap.add_argument("--time", type=int, default=200)
    ap.add_argument("--judge-depth", type=int, default=5)
    ap.add_argument("--out-dir", default="tournament")
    args = ap.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)
    model_paths = sorted(glob.glob(os.path.join(args.models_dir, "*.bin")))
    if len(model_paths) < 2:
        print("need at least 2 models", file=sys.stderr); sys.exit(1)

    rng = random.Random(42)
    for gen in range(args.generations):
        print("=== Generation {} ===".format(gen + 1))
        # scores
        scores = [0.0 for _ in model_paths]
        games_count = [0 for _ in model_paths]
        moves = [0 for _ in model_paths]
        subopt = [0 for _ in model_paths]
        for i in range(len(model_paths)):
            for j in range(i + 1, len(model_paths)):
                t = run_match(model_paths[i], model_paths[j], args.games, args.depth, args.time, args.judge_depth)
                games = t["games"] if t["games"] > 0 else 1
                # each invocation covered both sides with POSITIONS; scoreA for i
                scores[i] += t["scoreA"]
                scores[j] += t["scoreB"]
                games_count[i] += games
                games_count[j] += games
                moves[i] += t["movesA"]; moves[j] += t["movesB"]
                subopt[i] += t["suboptA"]; subopt[j] += t["suboptB"]
        # winrates
        winrate = []
        for i in range(len(model_paths)):
            g = games_count[i] if games_count[i] > 0 else 1
            wr = scores[i] / g
            rate = (subopt[i] / moves[i]) if moves[i] > 0 else 0.0
            winrate.append((wr, rate, i))
            print("model={} winrate={:.4f} games={} moves={} subopt={} subopt_rate={:.4f}".format(
                os.path.basename(model_paths[i]), wr, games_count[i], moves[i], subopt[i], rate))
        # select best: highest winrate, then lowest subopt rate, then index
        best_idx = sorted(range(len(model_paths)), key=lambda i: (-winrate[i][0], winrate[i][1], i))[0]
        best_path = model_paths[best_idx]
        print("best_model={} winrate={:.4f}".format(os.path.basename(best_path), winrate[best_idx][0]))

        # evolve: create next population from best if not last
        if gen < args.generations - 1:
            ver, n, dtype, vals = read_weights(best_path)
            new_paths = []
            var = 0.1
            for m in range(len(model_paths)):
                nw = [v + rng.gauss(0, var * max(1.0, abs(v))) for v in vals]
                nz = os.path.join(args.models_dir, "gen{}_model_{}.bin".format(gen + 2, m))
                write_weights(nz, ver, n, dtype, nw)
                new_paths.append(nz)
            # replace population: remove old gen? keep best? We'll set model_paths to new
            model_paths = sorted(new_paths)

    # final table
    print("\n=== Final model table ===")
    print("model games winrate moves subopt subopt_rate")
    for i in range(len(model_paths)):
        pass
    # recompute final? Already last gen printed. 
    # Write final weights reference
    print("final_best={}".format(os.path.basename(model_paths[0])))

if __name__ == "__main__":
    main()
