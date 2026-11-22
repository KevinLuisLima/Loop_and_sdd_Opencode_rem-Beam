#!/usr/bin/env python3
import os, sys, subprocess, random, argparse, struct

ENGINE = "./engine"

OPENINGS = [
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
    "rnbqkbnr/pppppppp/8/8/3P4/8/PPP1PPPP/RNBQKBNR b KQkq d3 0 1",
    "rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2",
]

try:
    import chess
    HAVE_CHESS = True
except Exception:
    HAVE_CHESS = False


def engine_move(fen, weights, time_ms):
    if weights is None:
        return None
    try:
        res = subprocess.run([ENGINE, fen, weights, str(time_ms)],
                             capture_output=True, text=True, timeout=30)
        if res.returncode == 0:
            return res.stdout.strip()
    except Exception:
        pass
    return None

def play_game(w_white, w_black, eng_time, judge_time, opening_seed):
    import chess
    random.seed(opening_seed)
    board = chess.Board(random.choice(OPENINGS))
    moves = []
    outcome = 0
    while True:
        if board.is_checkmate():
            # side to move lost
            outcome = -1 if board.turn == chess.WHITE else 1
            break
        if board.is_stalemate() or board.is_insufficient_material() or board.can_claim_fifty_moves() or board.can_claim_threefold_repetition():
            outcome = 0; break
        if len(moves) > 300:
            outcome = 0; break
        side = board.turn
        w = w_white if side == chess.WHITE else w_black
        fen = board.fen()
        mv = engine_move(fen, w, eng_time)
        if not mv:
            outcome = 0; break
        try:
            move = chess.Move.from_uci(mv)
        except ValueError:
            outcome = 0; break
        if move not in board.legal_moves:
            outcome = 0; break
        moves.append((side, mv, fen))
        board.push(move)
    # suboptimal recount using deeper search
    subopt = {w_white: 0, w_black: 0}
    move_counts = {w_white: 0, w_black: 0}
    for side, mv, fenpos in moves:
        w = w_white if side == chess.WHITE else w_black
        move_counts[w] += 1
        deep = engine_move(fenpos, w, judge_time)
        if deep is not None and deep != mv:
            subopt[w] += 1
    return outcome, move_counts, subopt


def mutate_model(src, dst, seed, var=0.05):
    data = open(src, 'rb').read()
    hdr = data[:13]
    fmt = '<%df' % (len(data[13:]) // 4)
    vals = list(struct.unpack(fmt, data[13:]))
    random.seed(seed)
    new = [v * (1 + random.uniform(-var, var)) for v in vals]
    with open(dst, 'wb') as f:
        f.write(hdr)
        f.write(struct.pack('<%df' % len(new), *new))


def main():
    if not HAVE_CHESS:
        print("python-chess required", file=sys.stderr); sys.exit(1)
    ap = argparse.ArgumentParser()
    ap.add_argument("--gens", type=int, default=5)
    ap.add_argument("--games-per-pair", type=int, default=2)
    ap.add_argument("--models-dir", default="weights")
    ap.add_argument("--time", type=int, default=100)
    ap.add_argument("--judge-time", type=int, default=400)
    ap.add_argument("--seed", type=int, default=42)
    args = ap.parse_args()
    random.seed(args.seed)

    current_models = sorted([os.path.join(args.models_dir, f) for f in os.listdir(args.models_dir) if f.endswith('.bin')])
    if len(current_models) < 2:
        print("need at least 2 models"); sys.exit(1)

    final_best = None
    for gen in range(args.gens):
        print(f"\n================ Generation {gen+1}/{args.gens} ================", flush=True)
        stats = {m: {"W":0,"D":0,"L":0,"played":0,"subopt":0,"moves":0} for m in current_models}
        # round robin
        for i in range(len(current_models)):
            for j in range(i+1, len(current_models)):
                a = current_models[i]; b = current_models[j]
                for g in range(args.games_per_pair):
                    for (w1, w2) in [(a, b), (b, a)]:
                        outcome, move_counts, subopt_counts = play_game(w1, w2, args.time, args.judge_time, args.seed + gen*10000 + i*100 + j*10 + g)
                        # outcome from white (w1) perspective
                        if outcome == 1:
                            stats[w1]["W"] += 1; stats[w2]["L"] += 1
                        elif outcome == -1:
                            stats[w1]["L"] += 1; stats[w2]["W"] += 1
                        else:
                            stats[w1]["D"] += 1; stats[w2]["D"] += 1
                        for m in (w1, w2):
                            stats[m]["played"] += 1
                            stats[m]["moves"] += move_counts[m]
                            stats[m]["subopt"] += subopt_counts[m]
        # print table
        print(f"{'model':45} {'P':>4} {'W':>4} {'D':>4} {'L':>4} {'win%':>7} {'moves':>6} {'subopt':>7} {'sub%':>6}")
        for m in current_models:
            s = stats[m]
            winpct = (s["W"] + 0.5*s["D"]) / s["played"] * 100 if s["played"] else 0
            subrate = (s["subopt"]/s["moves"]*100) if s["moves"] else 0
            print(f"{os.path.basename(m):45} {s['played']:4} {s['W']:4} {s['D']:4} {s['L']:4} {winpct:7.2f} {s['moves']:6} {s['subopt']:7} {subrate:6.2f}")
        # accounting invariants
        total_w = sum(stats[m]["W"] for m in current_models)
        total_l = sum(stats[m]["L"] for m in current_models)
        assert total_w == total_l, "accounting violation: wins != losses"
        for m in current_models:
            s = stats[m]
            assert s["W"] + s["D"] + s["L"] == s["played"], "accounting per model"
        print("Accounting invariants OK")

        # select best: highest winrate, then lowest subopt/move, then direct, then index
        def key(m):
            s = stats[m]
            winrate = (s["W"] + 0.5*s["D"]) / s["played"] if s["played"] else 0
            subrate = s["subopt"]/s["moves"] if s["moves"] else 1.0
            return (-winrate, subrate, current_models.index(m))
        best = min(current_models, key=key)
        best_stats = stats[best]
        best_winrate = (best_stats["W"] + 0.5*best_stats["D"]) / best_stats["played"] * 100
        best_subrate = (best_stats["subopt"]/best_stats["moves"]*100) if best_stats["moves"] else 0
        print(f"\nBest this gen: {os.path.basename(best)} winrate={best_winrate:.2f}% subopt/move={best_subrate:.2f}%")
        final_best = best

        # generate next population (except last gen)
        if gen < args.gens - 1:
            out_dir = os.path.join("tournament_out", f"gen{gen+1}")
            os.makedirs(out_dir, exist_ok=True)
            next_models = []
            for mi in range(len(current_models)):
                dst = os.path.join(out_dir, f"model_{mi}.bin")
                mutate_model(best, dst, args.seed + gen*999 + mi*7, var=0.05)
                next_models.append(dst)
            current_models = next_models

    print(f"\nFINAL BEST MODEL: {os.path.basename(final_best)}")
    print(f"Wrote variants under tournament_out/")


if __name__ == "__main__":
    main()
