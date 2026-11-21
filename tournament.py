#!/usr/bin/env python3
import argparse, os, random, re, subprocess, sys

ENGINE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "build", "engine")

def run_engine(fen, weights=None, time_ms=300):
    args = [ENGINE, fen]
    args.append(weights if weights else "")
    args.append(str(time_ms))
    try:
        p = subprocess.run(args, capture_output=True, text=True, timeout=max(5, time_ms/1000+5))
        out = p.stdout.strip().splitlines()
        return out[0].strip() if out else ""
    except Exception:
        return ""

def apply_move(fen, move):
    try:
        p = subprocess.run([ENGINE, "apply", fen, move], capture_output=True, text=True, timeout=5)
        out = p.stdout.strip()
        m = re.search(r"FEN=(.*) STATUS=(\w+)", out)
        if m:
            return m.group(1).strip(), m.group(2)
    except Exception:
        pass
    return fen, "error"

def list_models(models_dir):
    files = []
    for root,_,fn in os.walk(models_dir):
        for f in fn:
            if f.endswith(".weights"):
                files.append(os.path.join(root,f))
    files.sort()
    return files

def play_game(model_a, model_b, opening_fen, colors, engine_ms, judge_ms, seed, max_moves=300):
    # colors: (model_for_white, model_for_black)
    fen = opening_fen
    # map side to model index/weights
    # model 0 plays color of A maybe
    white_w = model_a if (colors==0) else model_b
    black_w = model_b if (colors==0) else model_a
    # careful: parameters model_a/b are weight paths; colors indicates whether A is white (0) or black (1)
    if colors == 0:
        white_w, black_w = model_a, model_b
    else:
        white_w, black_w = model_b, model_a
    moves = 0
    subopt = {white_w:0, black_w:0}
    moves_by = {white_w:0, black_w:0}
    mover = None
    while moves < max_moves:
        side_white = (fen.split()[1] == "w")
        wpath = white_w if side_white else black_w
        cur_model = wpath
        ref = run_engine(fen, None, judge_ms)
        guess = run_engine(fen, wpath, engine_ms)
        if guess in ("", "(none)"):
            # game end due to no legal moves
            break
        if ref and ref != guess:
            subopt[wpath] += 1
        mover = wpath
        fen_before = fen
        fen, status = apply_move(fen, guess)
        moves += 1
        moves_by[wpath] += 1
        if status == "mate":
            # mover wins
            winner = mover
            return winner, "win", moves, moves_by, subopt
        if status in ("stalemate","draw50","error"):
            return None, "draw", moves, moves_by, subopt
    return None, "draw", moves, moves_by, subopt

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--models-dir", default="models")
    ap.add_argument("--games-per-pair", type=int, default=1)
    ap.add_argument("--engine-ms", type=int, default=200)
    ap.add_argument("--judge-ms", type=int, default=500)
    ap.add_argument("--max-moves", type=int, default=300)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--generations", type=int, default=1)
    args = ap.parse_args()
    rng = random.Random(args.seed)
    models = list_models(args.models_dir)
    if len(models) < 2:
        print("Torneio requer >=2 modelos", file=sys.stderr); sys.exit(1)
    openings = [
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "rnbqkbnr/pppppppp/8/8/3P4/8/PPP1PPPP/RNBQKBNR b KQkq d3 0 1",
        "rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2",
    ]
    n = len(models)
    wins = [0]*n; draws_count=0; losses=[0]*n
    games_played = [0]*n
    subopt_total = [0]*n
    moves_total = [0]*n
    # head to head points
    h2h = [[0.0]*n for _ in range(n)]
    total_games = 0
    for gen in range(args.generations):
        for i in range(n):
            for j in range(i+1, n):
                for g in range(args.games_per_pair):
                    for color in (0,1):  # 0: i white, 1: j white
                        opening = rng.choice(openings)
                        ma, mb = models[i], models[j]
                        colors = color  # 0 means i white (A white), 1 means j white
                        winner, outcome, moves, moves_by, subopt = play_game(ma, mb, opening, colors, args.engine_ms, args.judge_ms, args.seed, args.max_moves)
                        total_games += 1
                        # update
                        if colors==0:
                            w_idx, b_idx = i, j
                        else:
                            w_idx, b_idx = j, i
                        if winner is None:
                            wins[w_idx]+=0; wins[b_idx]+=0
                            # draw: +0.5 each
                            h2h[w_idx][b_idx]+=0.5
                            h2h[b_idx][w_idx]+=0.5
                        elif winner == ma if (colors==0 and w_idx==i) else (winner == models[w_idx]):
                            # winner model path compare
                            pass
                        # Determine winner index by path
                        if winner is None:
                            draws_count += 1
                        else:
                            windex = None
                            for idxm,m in enumerate(models):
                                if m == winner:
                                    windex=idxm
                            if windex is not None:
                                # points
                                losser = b_idx if windex==w_idx else w_idx
                                wins[windex]+=1
                                losses[losser]+=1
                                h2h[windex][losser]+=1.0
                        # subopt
                        for idxm,m in enumerate(models):
                            if m in subopt:
                                subopt_total[idxm] += subopt[m]
                                moves_total[idxm] += moves_by.get(m,0)
                        games_played[w_idx]+=1
                        games_played[b_idx]+=1
        # progress minimal
    # print results
    print("Partidas jogadas: %d" % total_games)
    # Build rows
    rows=[]
    for idxm,m in enumerate(models):
        g = games_played[idxm]
        wr = (wins[idxm] + 0.5*(g - wins[idxm] - losses[idxm])) / g if g>0 else 0.0
        rate = (subopt_total[idxm]/moves_total[idxm]) if moves_total[idxm]>0 else 0.0
        draws = g - wins[idxm] - losses[idxm]
        rows.append((m, g, wins[idxm], draws, losses[idxm], wr, moves_total[idxm], subopt_total[idxm], rate))
    # header
    print("modelo partidas V E D winrate lances suboptimos taxa_sub")
    for r in rows:
        name = os.path.basename(r[0])
        print("%s %d %d %d %d %.4f %d %d %.6f" % (name, r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8]))
    # best model
    def sortkey(idxm):
        r=rows[idxm]
        g=r[1]
        # head-to-head vs possible ties handled manually; compare winrate, subopt rate, then index
        return (-r[5], r[8], idxm)
    best_idx = min(range(n), key=lambda i: (-(rows[i][5]), rows[i][8], i))
    # tie-break head-to-head among same winrate/subopt? simple
    # refine: if ties in winrate and rate, use h2h sum
    candidates = [i for i in range(n) if abs(rows[i][5]-rows[best_idx][5])<1e-9 and abs(rows[i][8]-rows[best_idx][8])<1e-9 and i!=best_idx]
    for c in candidates:
        if h2h[c][best_idx] > h2h[best_idx][c]:
            best_idx = c
    print("Melhor modelo: %s winrate=%.4f taxa_sub=%.6f partidas=%d" % (os.path.basename(models[best_idx]), rows[best_idx][5], rows[best_idx][8], rows[best_idx][1]))

if __name__ == "__main__":
    main()
