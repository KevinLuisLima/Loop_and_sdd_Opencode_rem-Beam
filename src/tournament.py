#!/usr/bin/env python3
import argparse, json, os, subprocess, time, math, random

try:
    import chess
except ImportError as e:
    raise SystemExit("python-chess required for tournament tool")

OPENINGS = [
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1",
    "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2",
    "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 2 2",
    "r1bqkb1r/pppp1ppp/2n2n2/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 3 3",
]

def run_engine(engine, weights, fen, time_ms, json_mode=True):
    cmd = [engine, "--weights", weights, "--fen", fen, "--time", str(time_ms)]
    if json_mode:
        cmd.append("--json")
    out = subprocess.run(cmd, capture_output=True, text=True)
    if out.returncode != 0:
        return None, None, 0.0, out.stderr.strip()
    txt = out.stdout.strip()
    if json_mode:
        try:
            d = json.loads(txt)
            return d.get("move"), d.get("score"), 0.0, ""
        except Exception:
            pass
    mv = txt.split()[0] if txt.split() else ""
    return mv, None, 0.0, ""

def apply_move(fen, uci):
    b = chess.Board(fen)
    b.push_uci(uci)
    return b.fen()

def play_game(engine, w1, w2, fen, time_ms, judge_time, threshold):
    board = chess.Board(fen)
    subopt = {w1:0, w2:0}
    mc = {w1:0, w2:0}
    mover = {True:w1, False:w2}
    ply = 0
    while not board.is_game_over() and ply < 120:
        side_white = board.turn
        w = mover[side_white]
        opp_fen = board.fen()
        mv, score, dt, err = run_engine(engine, w, opp_fen, time_ms)
        if not mv or mv == "0000":
            break
        try:
            m = chess.Move.from_uci(mv)
            if m not in board.legal_moves:
                break
            board.push(m)
        except Exception:
            break
        mc[w]+=1
        best_mv, best_score, _, _ = run_engine(engine, w, opp_fen, judge_time)
        if best_mv is not None and best_score is not None and best_mv != mv:
            try:
                new_fen = apply_move(opp_fen, mv)
                _, score_after, _, _ = run_engine(engine, w, new_fen, time_ms)
                if score_after is not None:
                    played_value = -score_after
                    if best_score - played_value > threshold:
                        subopt[w]+=1
            except Exception:
                pass
        ply += 1
    result = board.result() if board.is_game_over() else "1/2-1/2"
    winner = None
    if result == "1-0":
        winner = w1
    elif result == "0-1":
        winner = w2
    return winner, result, subopt, mc

def evaluate_population(engine, model_files, games_per_pair, time_ms):
    stats = {m: {"games":0,"wins":0,"draws":0,"losses":0,"moves":0,"subopt":0} for m in model_files}
    judge_time = max(time_ms*2, 100)
    threshold = 100
    for a in model_files:
        for b in model_files:
            if a == b:
                continue
            for g in range(games_per_pair):
                if g % 2 == 0:
                    w1, w2 = a, b
                else:
                    w1, w2 = b, a
                fen = OPENINGS[g % len(OPENINGS)]
                winner, result, subopt, mc = play_game(engine, w1, w2, fen, time_ms, judge_time, threshold)
                for m in (w1, w2):
                    stats[m]["moves"] += mc.get(m,0)
                    stats[m]["subopt"] += subopt.get(m,0)
                    stats[m]["games"] += 1
                if winner == w1:
                    stats[w1]["wins"] += 1
                    stats[w2]["losses"] += 1
                elif winner == w2:
                    stats[w2]["wins"] += 1
                    stats[w1]["losses"] += 1
                else:
                    stats[w1]["draws"] += 1
                    stats[w2]["draws"] += 1
    return stats

def mutate_weights(src_path, dst_path, variation, seed):
    import struct, random
    random.seed(seed)
    data = open(src_path,'rb').read()
    n = struct.unpack('<I', data[8:12])[0]
    vals = list(struct.unpack('<%df'%n, data[16:16+n*4]))
    for i in range(n):
        vals[i] += random.gauss(0.0, max(0.5, abs(vals[i])*variation))
    header = data[:16]
    open(dst_path,'wb').write(header + struct.pack('<%df'%n, *vals))

def select_best(stats):
    models = list(stats.keys())
    def key(m):
        s = stats[m]
        g = s["games"] or 1
        wr = (s["wins"] + 0.5*s["draws"]) / g
        sr = s["subopt"]/max(1,s["moves"])
        return (-wr, sr)
    return min(models, key=key)

def print_table(stats):
    print("\nModel            Games  W  D  L  Winrate   Moves  Subopt  Rate")
    for m in stats:
        s = stats[m]
        g = s["games"] or 1
        wr = (s["wins"] + 0.5*s["draws"]) / g
        sr = s["subopt"]/max(1,s["moves"])
        print(f"{os.path.basename(m):16} {s['games']:5} {s['wins']:2} {s['draws']:2} {s['losses']:2} {wr:8.3f} {s['moves']:6} {s['subopt']:6} {sr:6.4f}")
    total_w = sum(s["wins"] for s in stats.values())
    total_l = sum(s["losses"] for s in stats.values())
    print(f"Invariant: sum wins={total_w} sum losses={total_l}")

def run_tournament(args):
    model_files = sorted([os.path.join(args.models_dir, f) for f in os.listdir(args.models_dir) if f.endswith(".bin")])
    if len(model_files) < 2:
        raise SystemExit("tournament needs >=2 models")
    random.seed(args.seed)
    current = model_files
    best_overall = None
    for gen in range(args.generations):
        print(f"\n=== Generation {gen+1} ===")
        stats = evaluate_population(args.engine, current, args.games_per_pair, args.time_ms)
        print_table(stats)
        best = select_best(stats)
        print(f"Best this generation: {os.path.basename(best)}")
        best_overall = best
        if gen < args.generations-1:
            workdir = os.path.join(args.output, f"gen{gen+1}")
            os.makedirs(workdir, exist_ok=True)
            next_models = []
            for i in range(len(current)):
                dst = os.path.join(workdir, f"model_{i}.bin")
                mutate_weights(best, dst, args.variation, args.seed+gen*100+i)
                next_models.append(dst)
            current = next_models
    print(f"\nFinal best model: {best_overall}")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--models-dir", default="models")
    ap.add_argument("--engine", default="./build/chess")
    ap.add_argument("--games-per-pair", type=int, default=1)
    ap.add_argument("--time-ms", type=int, default=50)
    ap.add_argument("--generations", type=int, default=2)
    ap.add_argument("--variation", type=float, default=0.05)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--output", default="tournament_out")
    args = ap.parse_args()
    os.makedirs(args.output, exist_ok=True)
    run_tournament(args)

if __name__ == "__main__":
    main()
