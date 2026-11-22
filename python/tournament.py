#!/usr/bin/env python3
import argparse
import json
import math
import os
import random
import struct
import subprocess

ENGINE = "./build/chess"
FEATURES = 392

OPENINGS = [
 "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
 "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
 "rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 0 2",
 "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 3",
]

def read_weights(path):
    with open(path, "rb") as f:
        data = f.read()
    assert data[:4] == b"CHWT", "bad magic"
    ver, n, typ = struct.unpack("<III", data[4:16])
    assert ver == 1
    assert n == FEATURES
    vals = struct.unpack("<%df" % n, data[16:16 + 4 * n])
    return list(vals)

def write_weights(path, weights):
    with open(path, "wb") as f:
        f.write(b"CHWT")
        f.write(struct.pack("<III", 1, len(weights), 0))
        f.write(struct.pack("<%df" % len(weights), *weights))

def engine_move(fen, model, movetime, depth):
    args = [ENGINE, "--fen", fen, "--weights", model, "--movetime", str(movetime), "--depth", str(depth)]
    out = subprocess.check_output(args, timeout=60).decode().strip()
    return out.split()[0]

def engine_apply(fen, move):
    args = [ENGINE, "--fen", fen, "--apply-move", move]
    out = subprocess.check_output(args, timeout=60).decode().strip()
    return out

def play_game(w_model, b_model, opening, movetime, depth, judge_model, judge_depth, jug_time):
    fen = opening
    w_sub = 0
    b_sub = 0
    moves = 0
    white_is_w = True
    models = {"w": w_model, "b": b_model}
    while moves < 300:
        side = fen.split()[1]
        model = w_model if side == "w" else b_model
        player = "w" if side == "w" else "b"
        best = engine_move(fen, judge_model, jug_time, judge_depth)
        played = engine_move(fen, model, movetime, depth)
        appl = engine_apply(fen, played)
        if played != best:
            if player == "w":
                w_sub += 1
            else:
                b_sub += 1
        if player == "w":
            w_moves = 1
        else:
            pass
        moves += 1
        if appl.startswith("MATE"):
            winner = player
            return winner, w_sub, b_sub, moves
        if appl.startswith("DRAW"):
            return "draw", w_sub, b_sub, moves
        if appl.startswith("CONTINUE"):
            fen = appl[len("CONTINUE "):]
            continue
        if appl.strip() == "":
            return "draw", w_sub, b_sub, moves
        # fallback
        return "draw", w_sub, b_sub, moves
    return "draw", w_sub, b_sub, moves

def ci_95(w, d, n):
    if n == 0:
        return (0.0, 0.0)
    p = (w + 0.5 * d) / n
    se = math.sqrt(p * (1 - p) / n) if n > 0 else 0.0
    return (max(0.0, p - 1.96 * se), min(1.0, p + 1.96 * se))

def main():
    global ENGINE
    ap = argparse.ArgumentParser()
    ap.add_argument("--models-dir", default="models")
    ap.add_argument("--engine", default=ENGINE)
    ap.add_argument("--games-per-pair", type=int, default=2)
    ap.add_argument("--movetime", type=int, default=20)
    ap.add_argument("--depth", type=int, default=3)
    ap.add_argument("--judge-time", type=int, default=40)
    ap.add_argument("--judge-depth", type=int, default=5)
    ap.add_argument("--generations", type=int, default=5)
    ap.add_argument("--variation", type=float, default=0.08)
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--output", default="tournament_report.json")
    args = ap.parse_args()

    ENGINE = args.engine

    rng = random.Random(args.seed)
    models_dir = args.models_dir

    for gen in range(1, args.generations + 1):
        files = sorted([f for f in os.listdir(models_dir) if f.endswith(".bin")])
        paths = [os.path.join(models_dir, f) for f in files]
        n = len(paths)
        stats = {p: {"w": 0, "d": 0, "l": 0, "games": 0, "moves": 0, "subopt": 0, "name": os.path.basename(p)} for p in paths}

        for i in range(n):
            for j in range(i + 1, n):
                for g in range(args.games_per_pair):
                    # alternate colors
                    if g % 2 == 0:
                        white, black = paths[i], paths[j]
                    else:
                        white, black = paths[j], paths[i]
                    opening = rng.choice(OPENINGS)
                    judge = paths[0]
                    result, w_sub, b_sub, moves = play_game(white, black, opening, args.movetime, args.depth, judge, args.judge_depth, args.judge_time)
                    stats[white]["games"] += 1
                    stats[black]["games"] += 1
                    stats[white]["moves"] += max(1, moves // 2)
                    stats[black]["moves"] += max(1, moves // 2)
                    stats[white]["subopt"] += w_sub
                    stats[black]["subopt"] += b_sub
                    if result == "w":
                        stats[white]["w"] += 1
                        stats[black]["l"] += 1
                    elif result == "b":
                        stats[black]["w"] += 1
                        stats[white]["l"] += 1
                    else:
                        stats[white]["d"] += 1
                        stats[black]["d"] += 1

        # invariants
        tw = sum(s["w"] for s in stats.values())
        tl = sum(s["l"] for s in stats.values())
        assert tw == tl, "win/loss invariant failed"
        for p, s in stats.items():
            assert s["w"] + s["d"] + s["l"] == s["games"], "accounting failed for %s" % p

        # best
        best_path = sorted(paths, key=lambda p: (
            (stats[p]["w"] + 0.5 * stats[p]["d"]) / max(1, stats[p]["games"]),
            - (stats[p]["subopt"] / max(1, stats[p]["moves"])),
        ), reverse=True)[0]

        # report
        print("=== Generation %d ===" % gen)
        for p in paths:
            s = stats[p]
            g = max(1, s["games"])
            wr = (s["w"] + 0.5 * s["d"]) / g
            lo, hi = ci_95(s["w"], s["d"], g)
            sub = s["subopt"] / max(1, s["moves"])
            print("%-24s games=%d W=%d E=%d D=%d winrate=%.3f CI=[%.3f,%.3f] subopt/lance=%.4f" % (s["name"], s["games"], s["w"], s["d"], s["l"], wr, lo, hi, sub))
            if g < 10:
                print("  (insufficient games to distinguish)")
        print("Best:", os.path.basename(best_path))
        print("")

        # generate next population from best
        if gen < args.generations:
            base = read_weights(best_path)
            # backup best maybe keep file name best as new leader
            new_values = [base]
            for i in range(1, n):
                arr = [v * (1.0 + args.variation * (2.0 * rng.random() - 1.0)) + args.variation * 2.5 * (rng.random() - 0.5) for v in base]
                new_values.append(arr)
            for p, arr in zip(paths, new_values):
                write_weights(p, arr)

    # final report json
    final = {p: stats[p] for p in paths}
    with open(args.output, "w") as f:
        json.dump({"generations": args.generations, "stats": {os.path.basename(p): final[p] for p in paths}}, f, indent=2)
    print("Report written to", args.output)

if __name__ == "__main__":
    main()
