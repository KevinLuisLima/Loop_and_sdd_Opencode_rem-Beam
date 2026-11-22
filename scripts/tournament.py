import argparse, os, sys, subprocess, random, struct, math, time, copy

ENGINE=None
TIME_MS=100
DEPTH=8
JUDGE_TIME_MS=300
JUDGE_DEPTH=14
THRESHOLD_CP=50
OPENINGS=[
 "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
 "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3",
 "r1bqkb1r/pppp1ppp/2n2n2/4p3/4P3/2N2N2/PPPP1PPP/R1BQKB1R w KQkq - 4 4",
 "rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2",
]

PIECE_CHAR={'P':0,'N':1,'B':2,'R':3,'Q':4,'K':5}
def color_of(p): return 0 if p.isupper() else 1

class Board:
    def __init__(self, fen=None):
        if fen is None: fen=OPENINGS[0]
        self.grid=[[None]*8 for _ in range(8)]
        parts=fen.split()
        rows=parts[0].split('/')
        for ri,row in enumerate(rows):
            c=0
            for ch in row:
                if ch.isdigit(): c+=int(ch)
                else: self.grid[ri][c]=ch; c+=1
        self.side=0 if parts[1]=='w' else 1
        castle=parts[2] if len(parts)>2 else '-'
        self.cK='K' in castle; self.cQ='Q' in castle; self.ck='k' in castle; self.cq='q' in castle
        self.ep=None
        if len(parts)>3 and parts[3]!='-':
            f=parts[3]; col=ord(f[0])-ord('a'); row=8-int(f[1]); self.ep=(row,col)
        self.halfmove=int(parts[4]) if len(parts)>4 else 0
        self.fullmove=int(parts[5]) if len(parts)>5 else 1
        self.history=[]

    def clone(self):
        b=Board.__new__(Board); b.grid=[row[:] for row in self.grid]
        b.side=self.side; b.cK=self.cK; b.cQ=self.cQ; b.ck=self.ck; b.cq=self.cq
        b.ep=self.ep; b.halfmove=self.halfmove; b.fullmove=self.fullmove; b.history=list(self.history)
        return b

    def castle_str(self):
        s=''
        if self.cK: s+='K'
        if self.cQ: s+='Q'
        if self.ck: s+='k'
        if self.cq: s+='q'
        return s or '-'
    def ep_str(self):
        if self.ep is None: return '-'
        r,c=self.ep; return chr(ord('a')+c)+str(8-r)
    def pos_key(self):
        rows=[]
        for r in range(8):
            empty=0; s=''
            for c in range(8):
                p=self.grid[r][c]
                if p is None: empty+=1
                else:
                    if empty: s+=str(empty); empty=0
                    s+=p
            if empty: s+=str(empty)
            rows.append(s)
        return '/'.join(rows)+' '+'wb'[self.side]+' '+self.castle_str()+' '+self.ep_str()+' '+str(self.halfmove)

    def find_king(self, side):
        target='K' if side==0 else 'k'
        for r in range(8):
            for c in range(8):
                if self.grid[r][c]==target: return (r,c)
        return None

    def gen_pseudo(self, side=None):
        if side is None: side=self.side
        moves=[]
        for r in range(8):
            for c in range(8):
                p=self.grid[r][c]
                if p is None: continue
                if (0 if p.isupper() else 1)!=side: continue
                pt=p.upper()
                if pt=='P':
                    d=-1 if side==0 else 1
                    nr=r+d
                    if 0<=nr<8 and self.grid[nr][c] is None:
                        if (side==0 and nr==0) or (side==1 and nr==7):
                            for pr in (['Q','R','B','N'] if side==0 else ['q','r','b','n']):
                                moves.append((r,c,nr,c,pr))
                        else:
                            moves.append((r,c,nr,c,None))
                        sr=6 if side==0 else 1
                        if r==sr:
                            nr2=r+2*d
                            if 0<=nr2<8 and self.grid[nr2][c] is None and self.grid[r+d][c] is None:
                                moves.append((r,c,nr2,c,None))
                    for dc in (-1,1):
                        nc=c+dc; nr=r+d
                        if 0<=nr<8 and 0<=nc<8:
                            t=self.grid[nr][nc]
                            if t is not None and ((t.isupper())!=(side==0)):
                                if (side==0 and nr==0) or (side==1 and nr==7):
                                    for pr in (['Q','R','B','N'] if side==0 else ['q','r','b','n']):
                                        moves.append((r,c,nr,nc,pr))
                                else:
                                    moves.append((r,c,nr,nc,None))
                            if self.ep is not None and self.ep==(nr,nc):
                                moves.append((r,c,nr,nc,None))
                elif pt=='N':
                    for dr,dc in [(-2,-1),(-2,1),(-1,-2),(-1,2),(1,-2),(1,2),(2,-1),(2,1)]:
                        nr,nc=r+dr,c+dc
                        if 0<=nr<8 and 0<=nc<8:
                            t=self.grid[nr][nc]
                            if t is None or ((t.isupper())!=(side==0)):
                                moves.append((r,c,nr,nc,None))
                elif pt in ('B','R','Q'):
                    dirs=[]
                    if pt in ('B','Q'): dirs += [(-1,-1),(-1,1),(1,-1),(1,1)]
                    if pt in ('R','Q'): dirs += [(-1,0),(1,0),(0,-1),(0,1)]
                    for dr,dc in dirs:
                        nr,nc=r+dr,c+dc
                        while 0<=nr<8 and 0<=nc<8:
                            t=self.grid[nr][nc]
                            if t is None: moves.append((r,c,nr,nc,None))
                            elif ((t.isupper())!=(side==0)): moves.append((r,c,nr,nc,None)); break
                            else: break
                            nr+=dr; nc+=dc
                elif pt=='K':
                    for dr in (-1,0,1):
                        for dc in (-1,0,1):
                            if dr==0 and dc==0: continue
                            nr,nc=r+dr,c+dc
                            if 0<=nr<8 and 0<=nc<8:
                                t=self.grid[nr][nc]
                                if t is None or ((t.isupper())!=(side==0)):
                                    moves.append((r,c,nr,nc,None))
                    if side==0 and (r,c)==(7,4):
                        if self.cK and self.grid[7][5] is None and self.grid[7][6] is None:
                            moves.append((7,4,7,6,None))
                        if self.cQ and self.grid[7][3] is None and self.grid[7][2] is None and self.grid[7][1] is None:
                            moves.append((7,4,7,2,None))
                    if side==1 and (r,c)==(0,4):
                        if self.ck and self.grid[0][5] is None and self.grid[0][6] is None:
                            moves.append((0,4,0,6,None))
                        if self.cq and self.grid[0][3] is None and self.grid[0][2] is None and self.grid[0][1] is None:
                            moves.append((0,4,0,2,None))
        return moves

    def is_attacked(self, sq, side):
        for (r,c,tr,tc,pr) in self.gen_pseudo(side):
            if (tr,tc)==sq: return True
        return False
    def in_check(self, side):
        kp=self.find_king(side)
        if kp is None: return True
        return self.is_attacked(kp, 1-side)

    def apply_move(self, m):
        r,c,tr,tc,promo=m
        piece=self.grid[r][c]
        side=0 if piece.isupper() else 1
        captured=self.grid[tr][tc]
        ep_cap=False
        if piece.upper()=='P' and self.ep is not None and (tr,tc)==self.ep and captured is None:
            ep_cap=True; self.grid[r][tc]=None
        self.grid[tr][tc]=piece if promo is None else promo
        self.grid[r][c]=None
        if piece.upper()=='K':
            if c==4 and tc==6:
                self.grid[r][5]=self.grid[r][7]; self.grid[r][7]=None
            elif c==4 and tc==2:
                self.grid[r][3]=self.grid[r][0]; self.grid[r][0]=None
        if piece.upper()=='K':
            if side==0: self.cK=self.cQ=False
            else: self.ck=self.cq=False
        if piece.upper()=='R':
            if side==0:
                if (r,c)==(7,0): self.cQ=False
                if (r,c)==(7,7): self.cK=False
            else:
                if (r,c)==(0,0): self.cq=False
                if (r,c)==(0,7): self.ck=False
        if captured is not None and captured.upper()=='R':
            if (tr,tc)==(7,0): self.cQ=False
            if (tr,tc)==(7,7): self.cK=False
            if (tr,tc)==(0,0): self.cq=False
            if (tr,tc)==(0,7): self.ck=False
        if piece.upper()=='P' and abs(tr-r)==2:
            self.ep=((r+tr)//2,c)
        else:
            self.ep=None
        if piece.upper()=='P' or captured is not None or ep_cap:
            self.halfmove=0
        else:
            self.halfmove+=1
        if side==1: self.fullmove+=1
        self.side^=1

    def legal_moves(self):
        res=[]
        for m in self.gen_pseudo():
            b=self.clone()
            b.apply_move(m)
            mover=1-b.side
            kp=b.find_king(mover)
            if kp is not None and b.is_attacked(kp, b.side):
                continue
            res.append(m)
        return res

    def to_fen(self):
        rows=[]
        for r in range(8):
            empty=0; s=''
            for c in range(8):
                p=self.grid[r][c]
                if p is None: empty+=1
                else:
                    if empty: s+=str(empty); empty=0
                    s+=p
            if empty: s+=str(empty)
            rows.append(s)
        return ' '.join(['/'.join(rows), 'w' if self.side==0 else 'b', self.castle_str(), self.ep_str(), str(self.halfmove), str(self.fullmove)])

    def game_over(self):
        if not self.legal_moves():
            return 'mate' if self.in_check(self.side) else 'stalemate'
        if self.halfmove>=100: return 'draw50'
        if len(self.history)>=1:
            key=self.pos_key()
            cnt=sum(1 for h in self.history if h==key)
            if cnt>=2: return 'repetition'
        # insufficient
        nonking=[p for r in range(8) for c in range(8) for p in [self.grid[r][c]] if p is not None and p.upper()!='K']
        if len(nonking)==0: return 'insufficient'
        if len(nonking)<=1 and nonking and nonking[0].upper() in ('N','B'):
            return 'insufficient'
        if len(nonking)==2 and all(p.upper()=='B' for p in nonking):
            # same color? skip
            pass
        if self.fullmove>300:
            return 'draw300'
        return None

    def apply_uci(self, uci):
        fc=ord(uci[0])-ord('a'); fr=8-int(uci[1]); tc=ord(uci[2])-ord('a'); tr=8-int(uci[3])
        promo=None
        if len(uci)>=5:
            pc=uci[4]
            promo = pc.upper() if self.side==0 else pc.lower()
        for m in self.legal_moves():
            if (m[0],m[1],m[2],m[3])==(fr,fc,tr,tc) and (promo is None or m[4]==promo):
                self.history.append(self.pos_key())
                self.apply_move(m)
                return m
        raise ValueError("illegal move "+uci)

def engine_move(fen, weights, tm, dp):
    cmd=[ENGINE, "--fen", fen, "--weights", weights, "--time", str(tm), "--depth", str(dp)]
    try:
        out=subprocess.run(cmd, capture_output=True, text=True, timeout=(tm/1000.0+15))
    except Exception as e:
        raise RuntimeError(str(e))
    best=None; score=None
    for line in out.stdout.splitlines():
        toks=line.split()
        if len(toks)>=2 and toks[0]=="bestmove": best=toks[1]
        if len(toks)>=3 and toks[0]=="score":
            try: score=int(toks[2])
            except: pass
    return best, score, out.returncode

def fail(msg):
    print("ERROR: "+msg, file=sys.stderr); sys.exit(1)

def read_weights(path):
    import struct as st
    with open(path,'rb') as f: data=f.read()
    magic=data[0]|(data[1]<<8)|(data[2]<<16)|(data[3]<<24)
    version,count,typ=st.unpack_from("<III", data, 4)
    off=16
    vals=list(st.unpack("<%df"%count, data[off:off+count*4]))
    return vals

def write_weights(path, vals):
    import struct as st
    with open(path,"wb") as f:
        f.write(st.pack("<IIII", 0x31535754, 1, len(vals), 0))
        for v in vals: f.write(st.pack("<f", float(v)))

def perturb(vals, variation, rnd):
    out=list(vals)
    for i in range(len(out)):
        out[i]+=rnd.gauss(0.0, variation*max(1.0, abs(out[i])))
    return out

def play_game(model_white, model_black, opening, tm, dp, jtm, jdp):
    b=Board(opening)
    cur={0:model_white,1:model_black}
    positions=[] # (fen_before, side, played_uci, board_after)
    move_count=0
    while True:
        over=b.game_over()
        if over: break
        fen=b.to_fen()
        mv, score, rc = engine_move(fen, cur[b.side], tm, dp)
        if rc!=0 or mv is None:
            break
        snap=b.clone()
        b.history.append(b.pos_key())
        b.apply_uci(mv)
        after_fen=b.to_fen()
        mover_side = 1 - b.side
        positions.append((fen, mv, after_fen, mover_side))
        move_count+=1
        if move_count>1000: break
    # winner
    winner=-1
    over=b.game_over()
    if over=='mate':
        # side to move checkmated
        winner=1-b.side  # index 0/1? side to move's opponent
        # map side index: if white(0) is model 0, index by side
        # winner is side index (0 white,1 black)
    # else draw
    # subopt evaluation
    subopt_counts={0:0,1:0}
    moves_by_side={0:0,1:0}
    for (fen_before, mv, fen_after, mover_side) in positions:
        moves_by_side[mover_side]+=1
        # deep best on before
        bm, best_score, rc = engine_move(fen_before, JUDGE_WEIGHT, jtm, jdp)
        if rc!=0 or best_score is None:
            continue
        # after score
        om, after_score, rc2 = engine_move(fen_after, JUDGE_WEIGHT, jtm, jdp)
        if rc2!=0 or after_score is None:
            continue
        played_score = -after_score
        if best_score - played_score > THRESHOLD_CP:
            subopt_counts[mover_side]+=1
    return {
        'winner': winner,
        'move_count': move_count,
        'subopt_counts': subopt_counts,
        'moves_by_side': moves_by_side,
    }

# Need global judge weights
JUDGE_WEIGHT=None

def main():
    global ENGINE, TIME_MS, DEPTH, JUDGE_TIME_MS, JUDGE_DEPTH, JUDGE_WEIGHT
    ap=argparse.ArgumentParser()
    ap.add_argument("--binary", default="build/engine")
    ap.add_argument("--models", default="models")
    ap.add_argument("--time-ms", type=int, default=100)
    ap.add_argument("--depth", type=int, default=8)
    ap.add_argument("--judge-time-ms", type=int, default=300)
    ap.add_argument("--judge-depth", type=int, default=14)
    ap.add_argument("--generations", type=int, default=5)
    ap.add_argument("--games-per-pair", type=int, default=1)
    ap.add_argument("--variation", type=float, default=0.05)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--output", default="models_out")
    ap.add_argument("--threshold", type=int, default=50)
    ap.add_argument("--judge-model", default=None)
    ap.add_argument("--max-moves", type=int, default=1000)
    args=ap.parse_args()
    ENGINE=args.binary
    TIME_MS=args.time_ms
    DEPTH=args.depth
    JUDGE_TIME_MS=args.judge_time_ms
    JUDGE_DEPTH=args.judge_depth
    THRESHOLD_CP=args.threshold

    # collect initial models
    model_files=sorted([os.path.join(args.models, f) for f in os.listdir(args.models) if f.endswith(".wts")])
    if len(model_files)<2:
        fail("need at least 2 models in "+args.models)
    os.makedirs(args.output, exist_ok=True)
    # judge model selection
    if args.judge_model:
        JUDGE_WEIGHT=args.judge_model
    else:
        base=os.path.join(args.models, "model_base.wts")
        JUDGE_WEIGHT=base if os.path.exists(base) else model_files[0]

    rnd=random.Random(args.seed)
    population=list(model_files)
    gen_results=[]
    for g in range(args.generations):
        print("\n===== Generation %d =====" % g)
        # stats
        stats={p:{'name':os.path.basename(p),'games':0,'wins':0,'draws':0,'losses':0,'moves':0,'subopt':0} for p in population}
        head={ (a,b):0 for a in population for b in population if a!=b }
        n=len(population)
        total_games=0
        for i in range(n):
            for j in range(i+1,n):
                for gg in range(args.games_per_pair):
                    # opening choice
                    op=OPENINGS[(i*997+j*31+gg+g*17) % len(OPENINGS)]
                    # color alternation
                    if (gg % 2 == 0):
                        wmodel=population[i]; bmodel=population[j]
                        a_is_white=True
                    else:
                        wmodel=population[j]; bmodel=population[i]
                        a_is_white=False
                    res=play_game(wmodel,bmodel,op,TIME_MS,DEPTH,JUDGE_TIME_MS,JUDGE_DEPTH)
                    total_games+=1
                    wside=0; bside=1
                    # map winner side index to model path
                    if res['winner']==0:
                        # white won
                        winner_path=wmodel
                        loser_path=bmodel
                        stats[wmodel]['wins']+=1
                        stats[bmodel]['losses']+=1
                    elif res['winner']==1:
                        winner_path=bmodel
                        loser_path=wmodel
                        stats[bmodel]['wins']+=1
                        stats[wmodel]['losses']+=1
                    else:
                        stats[wmodel]['draws']+=1
                        stats[bmodel]['draws']+=1
                    for p in (wmodel,bmodel):
                        stats[p]['games']+=1
                    # moves and subopt
                    for pm in (wmodel,bmodel):
                        pass
                    for sd in (0,1):
                        mdl = wmodel if sd==0 else bmodel
                        stats[mdl]['moves'] += res['moves_by_side'][sd]
                        stats[mdl]['subopt'] += res['subopt_counts'][sd]
                    # head to head: winner gets credit
                    if res['winner']==0:
                        head[(wmodel,bmodel)] +=1 if wmodel!=bmodel else 0
                        # if wmodel is i side?
                    if res['winner']==1:
                        head[(bmodel,wmodel)] +=1
                    print("  gen%d pair(%s vs %s) game %d winner=%s moves=%d" % (g, os.path.basename(wmodel), os.path.basename(bmodel), gg, ('W' if res['winner']==0 else ('B' if res['winner']==1 else 'D')), res['move_count']))
        # winrate
        ranking=[]
        for p in population:
            s=stats[p]
            wr = (s['wins'] + 0.5*s['draws']) / max(1,s['games'])
            rate = s['subopt']/max(1,s['moves'])
            s['winrate']=wr
            s['subopt_rate']=rate
            ranking.append((p,s))
        # sort best first
        ranking.sort(key=lambda x:(-x[1]['winrate'], x[1]['subopt_rate'], population.index(x[0])))
        print("\nGeneration %d results:" % g)
        for p,s in ranking:
            ci = 1.96*math.sqrt(s['winrate']*(1-s['winrate'])/max(1,s['games']))
            suff = " (n<30, low confidence)" if s['games']<30 else ""
            print("  model=%s games=%d W=%d D=%d L=%d winrate=%.4f+/-%.4f moves=%d subopt=%d subopt_rate=%.4f%s" % (
                s['name'], s['games'], s['wins'], s['draws'], s['losses'], s['winrate'], ci, s['moves'], s['subopt'], s['subopt_rate'], suff))
        best_p=ranking[0][0]
        best_s=ranking[0][1]
        print("  BEST gen%d: %s winrate=%.4f subopt_rate=%.4f" % (g, best_s['name'], best_s['winrate'], best_s['subopt_rate']))
        gen_results.append((g, best_p, best_s['winrate']))
        # generate next population
        if g < args.generations-1:
            best_vals=read_weights(best_p)
            os.makedirs(os.path.join(args.output, "gen%d"%(g+1)), exist_ok=True)
            outdir=os.path.join(args.output, "gen%d"%(g+1))
            newpop=[]
            for i in range(len(population)):
                if i==0:
                    newvals=list(best_vals)
                else:
                    newvals=perturb(best_vals, args.variation, random.Random(args.seed*1000+g*10+i))
                path=os.path.join(outdir, "model_%d.wts"%i)
                write_weights(path, newvals)
                newpop.append(path)
            population=newpop
    # final best overall: highest winrate then subopt rate
    # Use last gen ranking (already). best from last generation already.
    best_last=gen_results[-1]
    print("\n===== FINAL BEST MODEL =====")
    print("best=%s winrate=%.4f" % (best_last[1], best_last[2]))
    # print subopt table for best model if last gen stats available
    print(" (see generation results above for subopt moves)")

if __name__=="__main__":
    main()
