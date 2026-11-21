import argparse
import subprocess
import sys

ENGINE = "./engine"

def call_engine(args):
    r = subprocess.run([ENGINE] + args, capture_output=True, text=True, timeout=15)
    if r.returncode != 0:
        return None
    return r.stdout.strip().split()

def apply_move_raw(fen, mv):
    r = subprocess.run([ENGINE, "--apply", fen, mv], capture_output=True, text=True, timeout=15)
    line = r.stdout.strip()
    if r.returncode != 0 or not line or line == "ERROR":
        return None
    return line

def apply_move(fen, mv):
    return apply_move_raw(fen, mv)

def terminal(fen):
    out = call_engine(["--terminal", fen])
    if not out:
        return 0
    try:
        return int(out[0])
    except ValueError:
        return 0

def game(engine_is_white, seed):
    fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    plies = 0
    while plies < 300:
        res = terminal(fen)
        if res != 0:
            # game should have ended; determine winner by who moved last? handled externally
            break
        white_turn = "w" in fen.split()[1]
        engine_turn = (engine_is_white and white_turn) or (not engine_is_white and not white_turn)
        if engine_turn:
            out = call_engine([fen])
            if not out:
                return "error", "engine no move"
            mv = out[0]
            if mv == "0000":
                # no legal move but terminal should be handled; treat as draw
                return "draw", "no move"
        else:
            out = call_engine(["--random", fen, str(seed + plies)])
            if not out:
                return "error", "random no move"
            mv = out[0]
            if mv == "0000":
                return "draw", "no move"
        newfen = apply_move(fen, mv)
        if newfen is None:
            return "illegal", "%s illegal move %s" % ("engine" if engine_turn else "random", mv)
        fen = newfen
        plies += 1
        # after move, check terminal
        tr = terminal(fen)
        if tr == 1:
            # side to move is mated => mover won
            winner_engine = engine_turn
            return ("win" if winner_engine else "loss"), "mate"
        if tr in (2, 3):
            return "draw", "terminal"
    if plies >= 300:
        return "draw", "300 moves"
    tr = terminal(fen)
    if tr == 1:
        # side to move mated (no move was made after? rare)
        return "draw", "unfinished"
    return "draw", "finished"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--games", type=int, default=100)
    args = ap.parse_args()
    illegal = 0
    wins = draws = losses = 0
    errors = 0
    for i in range(args.games):
        eng_white = (i % 2 == 0)
        result, reason = game(eng_white, 1000 + i * 97)
        if result == "illegal" or result == "error":
            errors += 1
            illegal += 1
            print("game", i, result, reason)
            break
        if result == "win":
            wins += 1
        elif result == "loss":
            losses += 1
        else:
            draws += 1
        if (i + 1) % 20 == 0:
            print("played %d: W=%d D=%d L=%d illegal=%d" % (i+1, wins, draws, losses, illegal))
    total = wins + draws + losses
    winrate = (wins + 0.5 * draws) / total if total else 0
    print("FINAL: games=%d wins=%d draws=%d losses=%d winrate=%.4f illegal=%d" % (total, wins, draws, losses, winrate, illegal))
    if illegal == 0 and winrate >= 0.95:
        print("PASS")
        sys.exit(0)
    print("FAIL")
    sys.exit(1)

if __name__ == "__main__":
    main()
PY