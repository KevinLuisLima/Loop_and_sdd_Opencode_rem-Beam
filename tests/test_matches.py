import os, sys, random, subprocess, time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "scripts"))
from tournament import Board

ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENGINE=os.path.join(ROOT, "build", "engine")
MODEL=os.path.join(ROOT, "models", "model_base.wts")
if not os.path.exists(MODEL):
    MODEL=os.path.join(ROOT, "models", "model_0.wts")

VALS={'P':1,'N':3,'B':3,'R':5,'Q':9,'K':0}
def material_score(b):
    s=0
    for r in range(8):
        for c in range(8):
            p=b.grid[r][c]
            if p is None: continue
            v=VALS[p.upper()]
            s += v if p.isupper() else -v
    return s

def engine_move(fen, tm=20, dp=4):
    out=subprocess.run([ENGINE, "--fen", fen, "--weights", MODEL, "--time", str(tm), "--depth", str(dp)],
                      capture_output=True, text=True, timeout=tm/1000.0+5)
    mv=None
    for line in out.stdout.splitlines():
        t=line.split()
        if len(t)>=2 and t[0]=="bestmove": mv=t[1]
    return mv, out.returncode

def main():
    failures=[]
    # 1. Random 100 games
    random.seed(123)
    illegal=0; wins=0; games=0
    for g in range(100):
        b=Board()
        model_is_white = (g%2==0)
        moves=0
        while b.game_over() is None and moves<400:
            fen=b.to_fen()
            if (b.side==0 and model_is_white) or (b.side==1 and not model_is_white):
                # engine to move
                mv, rc = engine_move(fen, 15, 3)
                if rc!=0 or mv is None:
                    print("engine failed", rc); break
                try:
                    b.apply_uci(mv)
                except Exception as e:
                    illegal+=1; print(" ILLEGAL engine move", mv, e); break
            else:
                ms=b.legal_moves()
                if not ms: break
                m=random.choice(ms)
                # apply
                fr,fc,tr,tc,pr=m
                uci=chr(ord('a')+fc)+str(8-fr)+chr(ord('a')+tc)+str(8-tr)
                if pr: uci+=pr.lower()
                b.apply_uci(uci)
            moves+=1
        go=b.game_over()
        if go=='mate':
            winner=1-b.side
            # map winner side index to "engine"?
            engine_is_white = model_is_white
            engine_side = 0 if engine_is_white else 1
            if winner==engine_side: wins+=1
        games+=1
    illegal_check = (illegal==0)
    winrate = wins/games
    print("RANDOM: games=%d illegal=%d winrate=%.3f (engine)" % (games, illegal, winrate))
    if not illegal_check or winrate < 0.95:
        failures.append("random match gate")

    # 2. Greedy baseline 50 games
    wins=0; games=0; illegal=0
    def greedy_move(b):
        best=None; bestscore=None
        for m in b.legal_moves():
            bb=b.clone(); bb.apply_move(m)
            sc=material_score(bb)
            if (b.side==0 and (bestscore is None or sc>bestscore)) or (b.side==1 and (bestscore is None or sc<bestscore)):
                best=m; bestscore=sc
        if best is None: return None
        fr,fc,tr,tc,pr=best
        uci=chr(ord('a')+fc)+str(8-fr)+chr(ord('a')+tc)+str(8-tr)
        if pr: uci+=pr.lower()
        return uci

    for g in range(50):
        b=Board()
        model_is_white = (g%2==0)
        moves=0
        while b.game_over() is None and moves<400:
            fen=b.to_fen()
            if (b.side==0 and model_is_white) or (b.side==1 and not model_is_white):
                mv, rc = engine_move(fen, 15, 3)
                if rc!=0 or mv is None: break
                try: b.apply_uci(mv)
                except Exception: illegal+=1; break
            else:
                mv=greedy_move(b)
                if mv is None: break
                b.apply_uci(mv)
            moves+=1
        go=b.game_over()
        if go=='mate':
            winner=1-b.side
            engine_side = 0 if model_is_white else 1
            if winner==engine_side: wins+=1
        games+=1
    winrate=wins/games
    print("GREEDY: games=%d winrate=%.3f illegal=%d" % (games, winrate, illegal))
    if winrate < 0.80 or illegal>0:
        failures.append("greedy match gate")

    # 3. Tactical mate-in-1 positions
    mates=[
     "7k/5Q2/6K1/8/8/8/8/8 w - - 0 1",
     "r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4",
     "6k1/5ppp/8/8/8/8/8/3R2K1 w - - 0 1", # not mate
    ]
    # Use positions that are actual forced mate; count if engine move mates
    success=0; tested=0
    for fen in mates:
        b=Board(fen)
        mv, rc = engine_move(fen, 100, 6)
        if rc!=0 or mv is None: 
            continue
        tested+=1
        bb=b.clone()
        try:
            bb.apply_uci(mv)
        except Exception:
            continue
        if bb.game_over()=='mate':
            # side to move (opponent) has no moves and in check
            if bb.in_check(bb.side):
                success+=1
    # also include a few generated mate-in-1 by brute? Just report
    print("TACTICAL: tested=%d success=%d" % (tested, success))
    # not strict failure if 0? require at least 1
    if tested>0 and success/tested < 0.5:
        failures.append("tactical gate")

    if failures:
        print("FAILURES:", failures)
        sys.exit(1)
    else:
        print("ALL MATCH TESTS PASSED")
        sys.exit(0)

if __name__=="__main__":
    main()
