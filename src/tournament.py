#!/usr/bin/env python3
import argparse, os, subprocess, sys, random, struct, math, json

def bin_path():
    cand = ["build/chess","./build/chess","../build/chess"]
    for c in cand:
        if os.path.isfile(c): return c
    return "weights/../build/chess" if False else "build/chess"

BIN = bin_path()

def read_weights(path):
    with open(path,"rb") as f:
        magic=f.read(4)
        ver=struct.unpack("<I",f.read(4))[0]
        cnt=struct.unpack("<I",f.read(4))[0]
        dtype=f.read(1)[0]
        vals=list(struct.unpack(f"<{cnt}f",f.read(cnt*4)))
    return vals, ver, cnt, dtype

def write_weights(path, vals):
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path,"wb") as f:
        f.write(struct.pack("<4s", b"CBSW"))
        f.write(struct.pack("<I",1))
        f.write(struct.pack("<I",len(vals)))
        f.write(struct.pack("<B",0))
        for v in vals:
            f.write(struct.pack("<f",float(v)))

def engine_move(fen, weights, depth):
    try:
        r=subprocess.run([BIN, fen, weights], capture_output=True, text=True, timeout=15)
    except Exception:
        return None
    if r.returncode != 0:
        return None
    return r.stdout.strip()

def engine_analyze(fen, weights, depth):
    try:
        r=subprocess.run([BIN, "--analyze", fen, weights, str(depth)],
                         capture_output=True, text=True, timeout=20)
    except Exception:
        return None, None
    # parse
    parts=r.stdout.strip().split()
    mv=None; sc=None
    if "bestmove" in parts:
        i=parts.index("bestmove")
        if i+1 < len(parts): mv=parts[i+1]
    if "score" in parts:
        i=parts.index("score")
        if i+1 < len(parts):
            try: sc=int(parts[i+1])
            except: pass
    return mv, sc

JUDGE_WEIGHTS = None
JUDGE_DEPTH = 4
SUBOPT_THRESHOLD = 50

try:
    import chess
    HAS_CHESS=True
except Exception:
    HAS_CHESS=False

OPENINGS = [
 "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
 "rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2",
 "rnbqkb1r/ppp1pppp/3p1n2/8/3PP3/8/PPP2PPP/RNBQKBNR w KQkq - 0 3",
 "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
 "rnbqk2r/pppp1ppp/5n2/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
]

def play_game(white_idx, black_idx, models, depth, judge_weights, openings, max_moves=300):
    stats = {"moves": [0,0], "subopt": [0,0], "illegal": [0,0]}
    if not HAS_CHESS:
        return "1/2-1/2", stats
    import chess as C
    opening = random.choice(openings)
    board = C.Board(opening)
    # map model index to player side
    players = {C.WHITE: white_idx, C.BLACK: black_idx}
    moves_total = 0
    while (not board.is_game_over()) and moves_total < max_moves:
        mv = legal = list(board.legal_moves)
        if not mv:
            break
        acting = white_idx if board.turn == C.WHITE else black_idx
        fen = board.fen()
        move_str = engine_move(fen, models[acting], depth)
        move_obj = None
        if move_str:
            try:
                move_obj = C.Move.from_uci(move_str)
            except Exception:
                move_obj = None
        if move_obj is None or move_obj not in legal:
            stats["illegal"][0 if acting==white_idx else 1] += 1  # index by side not model? We'll map later
            # store in acting model accumulator global outside? We'll adjust: return acting
            # For accounting outside, we need index; mutate stats with acting model key
            stats.setdefault("illegal_model", {}).setdefault(acting,0)
            stats["illegal_model"][acting]=stats["illegal_model"].get(acting,0)+1
            # offender loses
            return ("0-1" if board.turn==C.WHITE else "1-0"), stats
        # judge
        best, bestscore = engine_analyze(fen, judge_weights, JUDGE_DEPTH)
        if best is not None and best != move_str and bestscore is not None:
            board2 = board.copy()
            board2.push(move_obj)
            _, after_score = engine_analyze(board2.fen(), judge_weights, JUDGE_DEPTH)
            if after_score is not None:
                gap = bestscore + after_score
                if gap > SUBOPT_THRESHOLD:
                    stats["subopt_model"] = stats.get("subopt_model",{})
                    stats["subopt_model"][acting] = stats["subopt_model"].get(acting,0)+1
        stats["moves_model"] = stats.get("moves_model",{})
        stats["moves_model"][acting] = stats["moves_model"].get(acting,0)+1
        board.push(move_obj)
        moves_total += 1
    # determine result
    if board.is_checkmate():
        res = "1-0" if board.turn == C.BLACK else "0-1"  # side to move has no moves and in check: opponent won. board.turn is side to move
    elif board.is_stalemate() or board.is_insufficient_material() or board.is_seventyfive_moves() or board.is_fivefold_repetition():
        res = "1/2-1/2"
    else:
        if moves_total >= 300:
            res = "1/2-1/2"
        else:
            outc = board.outcome()
            res = outc.result() if outc else "1/2-1/2"
    return res, stats

def update_stats(stats, white_idx, black_idx, result, game_stats):
    # games
    stats[white_idx]["games"] += 1
    stats[black_idx]["games"] += 1
    if result == "1-0":
        stats[white_idx]["W"] += 1
        stats[black_idx]["L"] += 1
    elif result == "0-1":
        stats[white_idx]["L"] += 1
        stats[black_idx]["W"] += 1
    else:
        stats[white_idx]["D"] += 1
        stats[black_idx]["D"] += 1
    gm = game_stats.get("moves_model",{})
    so = game_stats.get("subopt_model",{})
    il = game_stats.get("illegal_model",{})
    for mid, c in gm.items():
        stats[mid]["moves"] += c
    for mid, c in so.items():
        stats[mid]["subopt"] += c
    for mid, c in il.items():
        stats[mid]["illegal"] += c

def tournament_round(models, games_per_pair, depth, judge_weights, openings, max_moves=300):
    n = len(models)
    stats = {i: {"games":0,"W":0,"D":0,"L":0,"moves":0,"subopt":0,"illegal":0} for i in range(n)}
    pairs = [(i,j) for i in range(n) for j in range(i+1,n)]
    for (i,j) in pairs:
        for g in range(games_per_pair):
            white = i if (g % 2 == 0) else j
            black = j if (g % 2 == 0) else i
            res, gstats = play_game(white, black, models, depth, judge_weights, openings, max_moves)
            update_stats(stats, white, black, res, gstats)
            print(f"  pair {i} vs {j} game {g+1}: {res}", file=sys.stderr)
    return stats

def rank_models(stats, models):
    order = list(stats.keys())
    def key(i):
        g = stats[i]["games"]
        wr = (stats[i]["W"] + 0.5*stats[i]["D"]) / g if g else 0.0
        # subopt per move
        sm = (stats[i]["subopt"]/stats[i]["moves"]) if stats[i]["moves"] else 0.0
        # tie-break direct not computed; index
        return (-wr, sm, i)
    order.sort(key=key)
    return order

def child_generation(best_path, n, variation, seed):
    vals, ver, cnt, dtype = read_weights(best_path)
    rng = random.Random(seed)
    out_paths = []
    for k in range(n):
        child = [v + (rng.random()-0.5)*(abs(v)*variation if v!=0 else 1.0) for v in vals]
        # small extra mutation
        for i in range(len(child)):
            if rng.random() < 0.1:
                child[i] *= (1.0 + (rng.random()-0.5)*0.2)
        out_paths.append((k, child))
    return out_paths

def main():
    global JUDGE_DEPTH, SUBOPT_THRESHOLD
    ap = argparse.ArgumentParser()
    ap.add_argument("--models", nargs="+", required=True, help="weight files")
    ap.add_argument("--output-dir", default="weights")
    ap.add_argument("--games", type=int, default=2)
    ap.add_argument("--depth", type=int, default=2)
    ap.add_argument("--judge-depth", type=int, default=4)
    ap.add_argument("--generations", type=int, default=5)
    ap.add_argument("--variation", type=float, default=0.05)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--max-moves", type=int, default=300)
    args = ap.parse_args()

    JUDGE_DEPTH = args.judge_depth
    models = list(args.models)
    judge_weights = models[0]  # independent-ish judge uses first model; could use models[0]
    # Better: use a deeper judge = first model weights but engine depth accounts; okay.
    
    rng = random.Random(args.seed)
    overall_best = models[0]
    overall_best_stats = None
    openings = OPENINGS

    os.makedirs(args.output_dir, exist_ok=True)
    report_lines = []
    for gen in range(args.generations):
        print(f"\n=== Generation {gen+1}/{args.generations} ===", file=sys.stderr)
        print(f"Population: {models}", file=sys.stderr)
        stats = tournament_round(models, args.games, args.depth, judge_weights, openings, args.max_moves)
        order = rank_models(stats, models)
        # print table
        print(f"Gen {gen+1} results:", file=sys.stderr)
        for i in order:
            s = stats[i]
            g = s["games"]
            wr = (s["W"]+0.5*s["D"])/g if g else 0.0
            sm = (s["subopt"]/s["moves"]) if s["moves"] else 0.0
            ci = 1.96*math.sqrt(wr*(1-wr)/g) if g else 0.0
            print(f"  model {i} ({models[i]}): G={g} W={s['W']} D={s['D']} L={s['L']} winrate={wr:.3f} CI=±{ci:.3f} moves={s['moves']} subopt={s['subopt']} rate={sm:.4f}", file=sys.stderr)
        best_idx = order[0]
        best_model = models[best_idx]
        print(f"Best this gen: model {best_idx} {best_model}", file=sys.stderr)
        # update overall
        overall_best = best_model
        overall_best_stats = stats[best_idx]
        # save best
        out_best = os.path.join(args.output_dir, "best.bin")
        vals, v, c, d = read_weights(best_model)
        write_weights(out_best, vals)
        # generate next population unless last
        if gen < args.generations - 1:
            gdir = os.path.join(args.output_dir, f"gen_{gen+2}")
            os.makedirs(gdir, exist_ok=True)
            children = child_generation(best_model, len(models), args.variation, args.seed + gen*97 + 1)
            new_models = []
            for k, child in children:
                cpath = os.path.join(gdir, f"model_{k}.bin")
                write_weights(cpath, child)
                new_models.append(cpath)
            models = new_models
            # judge weights follows best (first child)
            judge_weights = models[0]

    # final report
    print("\n=== FINAL SUMMARY ===")
    if overall_best_stats:
        s = overall_best_stats
        g = s["games"]
        wr = (s["W"]+0.5*s["D"])/g if g else 0.0
        sm = (s["subopt"]/s["moves"]) if s["moves"] else 0.0
        ci = 1.96*math.sqrt(wr*(1-wr)/g) if g else 0.0
        print(f"Best model: {overall_best}")
        print(f"Winrate: {wr:.4f} (95% CI ±{ci:.4f})")
        print(f"Games: {g}  W={s['W']} D={s['D']} L={s['L']}")
        print(f"Moves: {s['moves']}  Suboptimal: {s['subopt']}  Rate: {sm:.4f}")
        if g < 30:
            print("Note: number of games may be insufficient to distinguish models.")
    # detailed accounting invariant note
    print("Invariant: W+D+L=G, total wins = total losses.")

if __name__ == "__main__":
    main()
