#!/usr/bin/env python3
import argparse, os, sys, struct, random, subprocess, tempfile, urllib.request, math

HF_URL = "https://huggingface.co/datasets/linrock/bullet-training-data/resolve/main/S1/UHO.pdist.iter-1.bullet.bin.zst"
REC_SIZE = 32
REC_STRUCT = struct.Struct("<Q16BhBBB3B")

def sigmoid(x):
    if x >= 0:
        z = math.exp(-x); return 1.0/(1.0+z)
    else:
        z = math.exp(x); return z/(1.0+z)

def stream_records(max_records=20000, download_bytes=2000000):
    req = urllib.request.Request(HF_URL, headers={"Range": f"bytes=0-{download_bytes-1}"})
    try:
        resp = urllib.request.urlopen(req, timeout=60)
    except Exception as e:
        print("API INACCESSIBLE:", e, file=sys.stderr); sys.exit(1)
    data = resp.read()
    if len(data) == 0:
        print("Empty response from API", file=sys.stderr); sys.exit(1)
    with tempfile.NamedTemporaryFile(delete=False) as tmp:
        tmp.write(data); tmp_path = tmp.name
    proc = subprocess.run(["zstd", "-d", "-c", tmp_path], capture_output=True)
    os.unlink(tmp_path)
    raw = proc.stdout
    n = len(raw) // REC_SIZE
    if n == 0:
        print("No records decompressed", file=sys.stderr); sys.exit(1)
    n = min(n, max_records)
    recs = []
    for i in range(n):
        chunk = raw[i*REC_SIZE:(i+1)*REC_SIZE]
        occ, *rest = REC_STRUCT.unpack(chunk)
        recs.append((occ, list(rest[:16]), rest[16], rest[17]))
    return recs, HF_URL, n

def occ_to_pieces(occ, pcs):
    sqs = []
    o = occ
    while o:
        s = (o & -o).bit_length() - 1
        sqs.append(s); o &= o-1
    pieces = []
    idx = 0
    for s in sqs:
        nibble = pcs[idx // 2]
        piece = (nibble >> (4*(idx & 1))) & 0xF
        pieces.append((s, piece)); idx += 1
    return pieces

def features_from_record(rec):
    occ, pcs, score, result = rec
    pieces = occ_to_pieces(occ, pcs)
    pieces_map = {sq: pc for sq, pc in pieces}
    N = 395
    phi = [0.0]*N
    # material
    counts = [0]*5; opp=[0]*5
    for sq, pc in pieces:
        t = pc % 8
        if t > 4: continue
        color = 0 if pc < 8 else 1
        if color == 0: counts[t]+=1
        else: opp[t]+=1
    for t in range(5):
        phi[t] = counts[t]-opp[t]
    # PST
    for sq, pc in pieces:
        t = pc % 8
        if t > 5: continue
        color = 0 if pc < 8 else 1
        if color == 0:
            phi[5 + t*64 + sq] += 1.0
        else:
            phi[5 + t*64 + (sq ^ 56)] -= 1.0
    # mobility approx
    def attacks(sq, pc, occv):
        t = pc % 8; color = 0 if pc < 8 else 1
        r,c = divmod(sq,8); out=set()
        if t == 0:
            d = 1 if color == 0 else -1
            for dc in (-1,1):
                nc = c+dc; nr = r+d
                if 0 <= nr < 8 and 0 <= nc < 8: out.add(nr*8+nc)
        elif t == 1:
            for dr,dc in ((-2,-1),(-2,1),(-1,-2),(-1,2),(1,-2),(1,2),(2,-1),(2,1)):
                nr,nc = r+dr,c+dc
                if 0 <= nr < 8 and 0 <= nc < 8: out.add(nr*8+nc)
        elif t in (2,3,4,5):
            dirs = []
            if t == 2: dirs = ((-1,-1),(-1,1),(1,-1),(1,1))
            elif t == 3: dirs = ((-1,0),(1,0),(0,-1),(0,1))
            elif t == 4: dirs = ((-1,-1),(-1,0),(-1,1),(0,-1),(0,1),(1,-1),(1,0),(1,1))
            else: dirs = ((-1,-1),(-1,0),(-1,1),(0,-1),(0,1),(1,-1),(1,0),(1,1))
            for dr,dc in dirs:
                nr,nc = r+dr,c+dc
                while 0 <= nr < 8 and 0 <= nc < 8:
                    out.add(nr*8+nc)
                    s = nr*8+nc
                    if occv & (1<<s): break
                    nr+=dr; nc+=dc
        return out
    occv = occ
    w_att = set(); b_att = set()
    for sq, pc in pieces:
        a = attacks(sq, pc, occv)
        if pc < 8: w_att |= a
        else: b_att |= a
    mob_diff = len(w_att) - len(b_att)
    phi[389] = mob_diff/10.0

    # doubled
    white_pen = 0; black_pen = 0
    for f in range(8):
        wc = bc = 0
        for r in range(8):
            sq = r*8+f
            pc = pieces_map.get(sq)
            if pc is None: continue
            if pc % 8 == 0:
                if pc < 8: wc += 1
                else: bc += 1
        if wc > 1: white_pen += wc-1
        if bc > 1: black_pen += bc-1
    phi[390] = (black_pen - white_pen)

    # isolated
    white_iso = 0; black_iso = 0
    for sq, pc in pieces:
        if pc % 8 != 0: continue
        color_white = pc < 8
        f = sq % 8
        adj = False
        for ff in (f-1, f+1):
            if ff < 0 or ff > 7: continue
            for r in range(8):
                s2 = r*8+ff
                pc2 = pieces_map.get(s2)
                if pc2 is not None and (pc2 < 8) == color_white and (pc2 % 8 == 0):
                    adj = True; break
            if adj: break
        if not adj:
            if color_white: white_iso += 1
            else: black_iso += 1
    phi[391] = (black_iso - white_iso)

    # passed
    for sq, pc in pieces:
        if pc % 8 != 0: continue
        color_white = pc < 8
        f = sq % 8; r = sq // 8
        blocked = False
        for ff in (f-1, f, f+1):
            if ff < 0 or ff > 7: continue
            if color_white:
                rr = r+1; step = 1; end = 8
            else:
                rr = r-1; step = -1; end = -1
            while rr != end:
                s2 = rr*8+ff
                pc2 = pieces_map.get(s2)
                if pc2 is not None and (pc2 >= 8) == color_white and (pc2 % 8 == 0):
                    blocked = True; break
                rr += step
            if blocked: break
        if not blocked:
            adv = r if color_white else 7-r
            val = 1.0 + adv/4.0
            if color_white: phi[392] += val
            else: phi[392] -= val

    # bishop pair
    wb = sum(1 for sq, pc in pieces if pc < 8 and pc % 8 == 2)
    bb_ = sum(1 for sq, pc in pieces if pc >= 8 and pc % 8 == 2)
    bp = 0
    if wb >= 2: bp += 1
    if bb_ >= 2: bp -= 1
    phi[393] = bp

    # king safety
    def king_ring_attackers(king_white):
        # enemy pieces attacking adjacent ring
        ksq = None
        for sq, pc in pieces:
            if pc % 8 == 5 and ((pc < 8) == king_white):
                ksq = sq; break
        if ksq is None: return 0
        r, c = divmod(ksq, 8)
        ring = set()
        for dr in (-1,0,1):
            for dc in (-1,0,1):
                if dr==0 and dc==0: continue
                nr,nc = r+dr,c+dc
                if 0 <= nr < 8 and 0 <= nc < 8: ring.add(nr*8+nc)
        # ring includes king square? add it
        ring.add(ksq)
        cnt = 0
        for sq, pc in pieces:
            if (pc < 8) == king_white:
                continue
            a = attacks(sq, pc, occv)
            if a & ring:
                cnt += 1
        return cnt
    atk_white_king = king_ring_attackers(True)   # enemy (black) attackers vs white king
    atk_black_king = king_ring_attackers(False) # enemy (white) attackers vs black king
    phi[394] = (atk_black_king - atk_white_king) / 4.0

    y = result / 2.0
    return phi, y

def write_weights(path, weights):
    import struct
    with open(path, "wb") as f:
        f.write(struct.pack("<III B", 0x43485731, 1, len(weights), 0))
        for val in weights:
            f.write(struct.pack("<f", float(val)))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--num-models", type=int, default=5)
    ap.add_argument("--variation", type=float, default=0.1)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--out", default="weights")
    ap.add_argument("--samples", type=int, default=3000)
    args = ap.parse_args()
    random.seed(args.seed)
    os.makedirs(args.out, exist_ok=True)

    print("Downloading sample from", HF_URL, flush=True)
    recs, endpoint, used = stream_records(max_records=args.samples, download_bytes=2000000)
    print("Endpoint used:", endpoint)
    print("Records effectively used:", used)

    xs = []; ys = []
    for rec in recs:
        phi, y = features_from_record(rec)
        xs.append(phi); ys.append(y)
    n = len(xs)
    print("Training samples:", n)

    N = 395
    w = [0.0]*N
    w[0]=100.0; w[1]=300.0; w[2]=300.0; w[3]=500.0; w[4]=900.0
    w[389]=2.0; w[390]=-15.0; w[391]=-20.0; w[392]=25.0; w[393]=30.0; w[394]=30.0
    for sq in range(64):
        r,c = divmod(sq,8)
        center = (3.5-abs(r-3.5)) + (3.5-abs(c-3.5))
        w[5 + 1*64 + sq] = center*4.0
        w[5 + 2*64 + sq] = center*2.0
        w[5 + 0*64 + sq] = r*3.0

    scale = 1.0/400.0
    def loss(weights):
        total = 0.0
        for i in range(n):
            ev = 0.0
            xi = xs[i]
            for j in range(N):
                ev += weights[j]*xi[j]
            pred = sigmoid(ev*scale)
            y = ys[i]
            eps = 1e-9
            pred = min(max(pred, eps), 1-eps)
            total += -(y*math.log(pred)+(1-y)*math.log(1-pred))
        return total/n

    loss_before = loss(w)
    lr = 0.05; epochs = 5
    for epoch in range(epochs):
        grad = [0.0]*N
        for i in range(n):
            xi = xs[i]
            ev = 0.0
            for j in range(N):
                ev += w[j]*xi[j]
            pred = sigmoid(ev*scale)
            d = (pred - ys[i])*scale
            for j in range(N):
                grad[j] += d*xi[j]
        for j in range(N):
            w[j] -= lr*(grad[j]/n)
    loss_after = loss(w)
    print("Loss before:", loss_before)
    print("Loss after:", loss_after)

    models = []
    for mi in range(args.num_models):
        random.seed(args.seed + mi*997)
        mweights = [ww*(1.0+random.uniform(-args.variation,args.variation)) for ww in w]
        for j in range(N):
            mweights[j] += (mi - args.num_models//2)*0.5
        models.append(mweights)
        out_path = os.path.join(args.out, f"model_{mi}.bin")
        write_weights(out_path, mweights)
        print("Wrote", out_path)

    if models:
        write_weights("weights.bin", models[0])
        print("Wrote weights.bin")

    summary = os.path.join(args.out, "weights_summary.txt")
    with open(summary, "w") as f:
        f.write(f"endpoint={endpoint}\n")
        f.write(f"records_used={used}\n")
        f.write(f"loss_before={loss_before}\n")
        f.write(f"loss_after={loss_after}\n")
        f.write(f"num_models={args.num_models}\n")
        f.write(f"variation={args.variation}\n")
        f.write(f"seed={args.seed}\n")

if __name__ == "__main__":
    main()
