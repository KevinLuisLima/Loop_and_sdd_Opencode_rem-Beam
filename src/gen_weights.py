#!/usr/bin/env python3
import argparse, os, struct, subprocess, sys, tempfile, urllib.request, random, math

ENDPOINT = "https://huggingface.co/datasets/linrock/bullet-training-data/resolve/main/S1/UHO.pdist.iter-1.bullet.bin.zst"

W_COUNT = 392
# index helpers
MAT = {"p":0,"n":1,"b":2,"r":3,"q":4}
def pst_idx(pt, sq):
    return 5 + (pt-1)*64 + sq
DOUBLED = 5 + 6*64
ISOLATED = 6 + 6*64
MOBILITY = 7 + 6*64

PTYPES = {1:"p",2:"n",3:"b",4:"r",5:"q",6:"k"}

def fetch_decompressed(num_records, tmpdir):
    zst = os.path.join(tmpdir, "sample.zst")
    out = os.path.join(tmpdir, "sample.bin")
    # fetch 6 MB
    end = 6*1024*1024 - 1
    req = urllib.request.Request(ENDPOINT, headers={"Range": f"bytes=0-{end}"})
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            data = r.read()
    except Exception as e:
        raise SystemExit(f"API inacessivel: {e}")
    if not data:
        raise SystemExit("Resposta vazia da API")
    with open(zst, "wb") as f:
        f.write(data)
    # decompress (partial allowed; zstd may return error code but produce file)
    with open(out,"wb") as fo:
        subprocess.run(["zstd","-dq","-c",zst], stdout=fo, stderr=subprocess.DEVNULL, check=False)
    if not os.path.exists(out):
        raise SystemExit("Falha na descompressao")
    with open(out,"rb") as f:
        return f.read()

def decode_record(rec):
    # returns list of (sq, piece_code) and result
    occ = int.from_bytes(rec[0:8], "little")
    pcs = rec[8:24]
    score, result, ksq, oppksq = struct.unpack("<hBBB", rec[24:29])
    # pieces
    pieces = []
    idx = 0
    for sq in range(64):
        if not (occ & (1<<sq)):
            continue
        byte = pcs[idx//2]
        code = (byte >> (4*(idx%2))) & 0xF
        idx += 1
        pieces.append((sq, code))
    pieces.append((ksq, 6))
    pieces.append((oppksq, 14))
    return pieces, result

def parse_sample(data, n):
    rec_size = 32
    maxn = len(data)//rec_size
    n = min(n, maxn)
    out = []
    for i in range(n):
        rec = data[i*rec_size:(i+1)*rec_size]
        if len(rec) < rec_size:
            break
        pieces, result = decode_record(rec)
        # skip invalid: no king
        if len([c for (_,c) in pieces if (c&7)==6]) < 2:
            continue
        out.append((pieces, result))
    return out

def popcount(x):
    return bin(x).count("1")

def sliding_attacks(sq, occ, dirs):
    res = 0
    r, c = divmod(sq, 8)
    for dr, dc in dirs:
        nr, nc = r+dr, c+dc
        while 0 <= nr < 8 and 0 <= nc < 8:
            s = nr*8+nc
            res |= (1<<s)
            if (occ >> s) & 1:
                break
            nr += dr; nc += dc
    return res

KNIGHT_DIRS = [(-2,-1),(-2,1),(-1,-2),(-1,2),(1,-2),(1,2),(2,-1),(2,1)]
def knight_attacks(sq):
    r,c = divmod(sq,8); res=0
    for dr,dc in KNIGHT_DIRS:
        nr,nc=r+dr,c+dc
        if 0<=nr<8 and 0<=nc<8: res |= 1<<(nr*8+nc)
    return res

BISHOP_DIRS = [(1,1),(1,-1),(-1,1),(-1,-1)]
ROOK_DIRS = [(1,0),(-1,0),(0,1),(0,-1)]
QUEEN_DIRS = BISHOP_DIRS+ROOK_DIRS

def mobility_for(pieces, occ, color):
    att = 0
    for sq,code in pieces:
        if (code & 8) != (0 if color==0 else 8):
            continue
        pt = code & 7
        if pt == 1: # pawns not counted
            continue
        if pt == 2:
            att |= knight_attacks(sq)
        elif pt == 3:
            att |= sliding_attacks(sq, occ, BISHOP_DIRS)
        elif pt == 4:
            att |= sliding_attacks(sq, occ, ROOK_DIRS)
        elif pt == 5:
            att |= sliding_attacks(sq, occ, QUEEN_DIRS)
        elif pt == 6:
            # king not mobility
            pass
    own_occ = 0
    for sq,code in pieces:
        if (code&8) == (0 if color==0 else 8):
            own_occ |= (1<<sq)
    mob = att & ~own_occ
    # also not occupied by? all attacked squares excluding own
    return popcount(mob)

def features(pieces, result):
    N = W_COUNT
    x = [0.0]*N
    # material + PST
    for sq,code in pieces:
        pt = code & 7
        if pt < 1 or pt > 6:
            continue
        color = 0 if (code & 8)==0 else 1
        if pt != 6:
            mi = 0 if pt==1 else (1 if pt==2 else (2 if pt==3 else (3 if pt==4 else 4)))
            if color == 0:
                x[mi] += 1
            else:
                x[mi] -= 1
        pst_sq = sq if color==0 else sq^56
        idx = 5 + (pt-1)*64 + pst_sq
        if color == 0:
            x[idx] += 1
        else:
            x[idx] -= 1
    # pawn structure
    white_files=[0]*8
    black_files=[0]*8
    for sq,code in pieces:
        if (code&7)==1:
            if (code&8)==0:
                white_files[sq&7]+=1
            else:
                black_files[sq&7]+=1
    for f in range(8):
        if white_files[f]>1:
            x[DOUBLED] -= (white_files[f]-1)
        if black_files[f]>1:
            x[DOUBLED] += (black_files[f]-1)
    for f in range(8):
        if white_files[f]==0:
            continue
        left = white_files[f-1] if f>0 else 0
        right = white_files[f+1] if f<7 else 0
        if left==0 and right==0:
            x[ISOLATED] -= white_files[f]
    for f in range(8):
        if black_files[f]==0:
            continue
        left = black_files[f-1] if f>0 else 0
        right = black_files[f+1] if f<7 else 0
        if left==0 and right==0:
            x[ISOLATED] += black_files[f]
    # mobility
    occ = 0
    for sq,code in pieces:
        occ |= (1<<sq)
    white_mob = mobility_for(pieces, occ, 0)
    black_mob = mobility_for(pieces, occ, 1)
    x[MOBILITY] += (white_mob - black_mob)
    return x

def sigmoid(z):
    if z >= 0:
        ez = math.exp(-z); return 1/(1+ez)
    else:
        ez = math.exp(z); return ez/(1+ez)

def loss(samples, w):
    tot = 0.0
    for x, y in samples:
        e = sum(w[i]*x[i] for i in range(len(w)))
        p = sigmoid(e/400.0)
        # clamp
        p = min(max(p,1e-7),1-1e-7)
        tot -= (y*math.log(p) + (1-y)*math.log(1-p))
    return tot/len(samples)

def train(samples, w0, epochs, lr, rng):
    w = list(w0)
    # shuffle
    rng.shuffle(samples)
    before = loss(samples, w)
    N = len(samples)
    for ep in range(epochs):
        grad = [0.0]*len(w)
        for x,y in samples:
            e = sum(w[i]*x[i] for i in range(len(w)))
            p = sigmoid(e/400.0)
            diff = (p-y)/400.0
            for i in range(len(w)):
                grad[i] += diff*x[i]
        for i in range(len(w)):
            w[i] -= lr*grad[i]/N
    after = loss(samples, w)
    return w, before, after

def make_model_name(outdir, i):
    return os.path.join(outdir, ( "default.bin" if i==0 else f"model_{i}.bin"))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--models", type=int, default=4)
    ap.add_argument("--variation", type=float, default=0.05)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--output", default="weights")
    ap.add_argument("--samples", type=int, default=4096)
    args = ap.parse_args()

    os.makedirs(args.output, exist_ok=True)
    tmpdir = tempfile.mkdtemp()
    data = fetch_decompressed(args.samples, tmpdir)
    parsed = parse_sample(data, args.samples)
    if len(parsed) < 50:
        raise SystemExit(f"Amostra insuficiente: {len(parsed)} registros")
    # build samples
    samples = []
    for pieces, result in parsed:
        x = features(pieces, result)
        # target outcome from stm perspective
        if result == 2:
            y = 1.0
        elif result == 1:
            y = 0.5
        else:
            y = 0.0
        samples.append((x,y))
    rng = random.Random(args.seed)
    # base reference weights
    base = [0.0]*W_COUNT
    # material defaults
    base[0]=100.0; base[1]=320.0; base[2]=330.0; base[3]=500.0; base[4]=900.0
    # PST small random base
    for i in range(5, 5+6*64):
        base[i] = (rng.random()-0.5)*20.0
    # terms
    base[DOUBLED] = 15.0
    base[ISOLATED] = 20.0
    base[MOBILITY] = 3.0

    print(f"registros lidos={len(parsed)} endpoint={ENDPOINT}")
    for m in range(args.models):
        rr = random.Random(args.seed + 100*m)
        # subset
        sub = list(samples)
        rr.shuffle(sub)
        sub = sub[:min(len(sub), args.samples)]
        # initial weights
        w0 = [v + (rr.random()-0.5)* (abs(v)*args.variation if v!=0 else 1.0) for v in base]
        epochs = 100
        lr = 5.0 if m%2==0 else 3.0
        w, before, after = train(sub, w0, epochs, lr, rr)
        # variation label
        print(f"modelo {m}: amostras={len(sub)} perda_inicial={before:.6f} perda_final={after:.6f} epocas={epochs}")
        # write
        path = make_model_name(args.output, m)
        with open(path, "wb") as f:
            f.write(struct.pack("<4s", b"CBSW"))
            f.write(struct.pack("<I", 1))
            f.write(struct.pack("<I", W_COUNT))
            f.write(struct.pack("<B", 0))
            for val in w:
                f.write(struct.pack("<f", float(val)))
        print(f"escrito {path}")

if __name__ == "__main__":
    main()
