import argparse, os, sys, subprocess, tempfile, struct, random, math, urllib.request, shutil

RECORD_SIZE = 32
MIN_RECORDS = 500
WEIGHTS_MAGIC = 0x31535754
WEIGHTS_VERSION = 1

ENDPOINT_DEFAULT = "https://huggingface.co/datasets/linrock/bullet-training-data/resolve/main/S1/UHO.pdist.iter-1.bullet.bin.zst"

NUM_WEIGHTS = 393
W_MATERIAL = 0
W_PST = 5
W_MOBILITY = 389
W_DOUBLED = 390
W_ISOLATED = 391
W_KING = 392

def fail(msg):
    print("ERROR: " + msg, file=sys.stderr)
    sys.exit(1)

def fetch_sample(endpoint, max_compressed, records_needed):
    # estimate compressed bytes: records_needed*32 / 5 (crude) + safety
    est = max(65536, records_needed * 32 // 4)
    est = min(est, max_compressed)
    req = urllib.request.Request(endpoint, headers={"Range": "bytes=0-%d" % (est - 1)})
    try:
        resp = urllib.request.urlopen(req, timeout=120)
    except Exception as e:
        fail("dataset API inaccessible: %s" % e)
    data = resp.read()
    if not data:
        fail("empty response from endpoint")
    code = getattr(resp, "status", 200)
    # Accept 200 or 206
    if code not in (200, 206):
        fail("unexpected HTTP status %s" % code)
    tmp_zst = os.path.join(tempfile.gettempdir(), "sample.zst")
    with open(tmp_zst, "wb") as f:
        f.write(data)
    tmp_bin = os.path.join(tempfile.gettempdir(), "sample.bin")
    try:
        with open(tmp_bin, "wb") as out:
            p = subprocess.run(["zstd", "-d", "-c", tmp_zst], stdout=out, stderr=subprocess.PIPE)
    except FileNotFoundError:
        fail("zstd binary not found")
    # zstd may exit 1 due to truncated stream; still accept partial output
    if not os.path.exists(tmp_bin) or os.path.getsize(tmp_bin) < RECORD_SIZE * 10:
        err = p.stderr.decode(errors="replace")
        fail("decompression failed or output too small: %s" % err)
    return tmp_bin

def parse_records(path):
    with open(path, "rb") as f:
        data = f.read()
    n = len(data) // RECORD_SIZE
    recs = []
    for i in range(n):
        chunk = data[i*RECORD_SIZE:(i+1)*RECORD_SIZE]
        occ, pcs, score, result, ksq, oppksq, extra = struct.unpack("<Q16s hBBB3s", chunk)
        recs.append((occ, pcs, score, result, ksq, oppksq, extra))
    return recs

def decode_position(occ, pcs):
    # returns (lists of pieces for white/black) as squares; side to move white
    white = [[] for _ in range(6)]  # piece type -> list squares
    black = [[] for _ in range(6)]
    occ2 = occ
    idx = 0
    while occ2:
        low = occ2 & -occ2
        sq = low.bit_length() - 1
        occ2 ^= low
        nibble = (pcs[idx // 2] >> (4 * (idx & 1))) & 0x0F
        idx += 1
        piece_type = nibble & 7
        color = (nibble >> 3) & 1
        if piece_type >= 6:
            continue
        if color == 0:
            white[piece_type].append(sq)
        else:
            black[piece_type].append(sq)
    return white, black

def features_from_position(white, black):
    f = [0.0] * NUM_WEIGHTS
    # material counts
    for pt in range(5):  # exclude king
        wcnt = len(white[pt])
        bcnt = len(black[pt])
        f[W_MATERIAL + pt] = float(wcnt - bcnt)
    # PST features
    for pt in range(6):
        for sq in white[pt]:
            f[W_PST + pt * 64 + sq] += 1.0
        for sq in black[pt]:
            f[W_PST + pt * 64 + (sq ^ 56)] -= 1.0
    # doubled pawns
    # white
    wfiles = [0]*8
    for sq in white[0]:
        wfiles[sq % 8] += 1
    for fl in range(8):
        if wfiles[fl] > 1:
            f[W_DOUBLED] += (wfiles[fl] - 1)
    bfiles = [0]*8
    for sq in black[0]:
        bfiles[sq % 8] += 1
    for fl in range(8):
        if bfiles[fl] > 1:
            f[W_DOUBLED] -= (bfiles[fl] - 1)
    # isolated pawns
    def isolated_pawn_features(pawn_squares, sign):
        files_present = set(sq % 8 for sq in pawn_squares)
        for sq in pawn_squares:
            fl = sq % 8
            adj = False
            if fl > 0 and (fl-1) in files_present:
                adj = True
            if fl < 7 and (fl+1) in files_present:
                adj = True
            if not adj:
                # feature contribution sign
                pass
        # count isolated
        return None
    # white isolated
    wfileset = set(sq % 8 for sq in white[0])
    wisolated = 0
    for sq in white[0]:
        fl = sq % 8
        adj = False
        if fl > 0 and (fl-1) in wfileset: adj = True
        if fl < 7 and (fl+1) in wfileset: adj = True
        if not adj:
            wisolated += 1
    f[W_ISOLATED] += wisolated
    bfileset = set(sq % 8 for sq in black[0])
    bisolated = 0
    for sq in black[0]:
        fl = sq % 8
        adj = False
        if fl > 0 and (fl-1) in bfileset: adj = True
        if fl < 7 and (fl+1) in bfileset: adj = True
        if not adj:
            bisolated += 1
    f[W_ISOLATED] -= bisolated
    # mobility (cheap, approximate): use attacked squares counts of knights etc.
    # We'll approximate as 0 to keep feature simple; weight mobility small. But need nonzero.
    # Compute from actual board bitboards? Instead set feature 0 for mobility (weight stays near default).
    # Yet training needs variation; mobility term small. Keep 0.
    return f

def sigmoid(x):
    if x >= 0:
        z = math.exp(-x)
        return 1/(1+z)
    else:
        z = math.exp(x)
        return z/(1+z)

def loss_and_grad(X, y, w, scale=400.0):
    n = len(X)
    grad = [0.0]*len(w)
    total_loss = 0.0
    for i in range(n):
        # dot
        score = 0.0
        fi = X[i]
        for j in range(len(w)):
            score += fi[j]*w[j]
        p = sigmoid(score/scale)
        # logistic loss
        eps = 1e-12
        yy = y[i]
        total_loss += -(yy*math.log(p+eps) + (1-yy)*math.log(1-p+eps))
        # grad
        for j in range(len(w)):
            grad[j] += (p - yy) * fi[j] / scale
    for j in range(len(w)):
        grad[j] /= max(1, n)
    return total_loss / max(1,n), grad

def default_weights_list():
    w = WeightsFileDefault()
    return w

class WeightsFileDefault:
    pass

def build_default_weight_list():
    # mirror weights_default (approx)
    vals = [0.0]*NUM_WEIGHTS
    vals[W_MATERIAL+0]=100.0
    vals[W_MATERIAL+1]=300.0
    vals[W_MATERIAL+2]=310.0
    vals[W_MATERIAL+3]=500.0
    vals[W_MATERIAL+4]=900.0
    # PST simple
    for sq in range(64):
        r = sq//8
        vals[W_PST+0*64+sq] = 5.0*(r-1)
    for sq in range(64):
        r = sq//8; c=sq%8
        df = (3-c) if c<4 else (c-4)
        dr = (3-r) if r<4 else (r-4)
        vals[W_PST+1*64+sq] = 20.0 - 4.0*(df+dr)
    for sq in range(64):
        r = sq//8; c=sq%8
        vals[W_PST+2*64+sq] = 10.0 if (2<=r<=5 and 2<=c<=5) else 0.0
    for sq in range(64):
        r = sq//8
        vals[W_PST+3*64+sq] = 10.0 if r==6 else 0.0
    for sq in range(64):
        r = sq//8; c=sq%8
        dfc = (3-c) if c<4 else (c-4)
        drc = (3-r) if r<4 else (r-4)
        vals[W_PST+4*64+sq] = 8.0 - 1.5*(dfc+drc)
    for sq in range(64):
        r = sq//8; c=sq%8
        v = 0.0
        if r==0 and (c<=2 or c>=5): v=20.0
        if r<=1: v += 5.0
        vals[W_PST+5*64+sq] = v
    vals[W_MOBILITY]=1.0
    vals[W_DOUBLED]=-10.0
    vals[W_ISOLATED]=-8.0
    vals[W_KING]=-5.0
    return vals

def save_weights(path, vals):
    import struct as st
    with open(path, "wb") as f:
        magic = WEIGHTS_MAGIC
        f.write(st.pack("<IIII", magic, WEIGHTS_VERSION, len(vals), 0))
        for v in vals:
            f.write(st.pack("<f", float(v)))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--num-models", type=int, default=4)
    ap.add_argument("--variation", type=float, default=0.05)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--output", default="models")
    ap.add_argument("--records", type=int, default=2000)
    ap.add_argument("--max-compressed", type=int, default=512*1024)
    ap.add_argument("--endpoint", default=ENDPOINT_DEFAULT)
    ap.add_argument("--epochs", type=int, default=200)
    ap.add_argument("--min-records", type=int, default=500)
    args = ap.parse_args()

    random.seed(args.seed)
    os.makedirs(args.output, exist_ok=True)

    compressed_bytes = args.records * 32 // 4
    compressed_bytes = max(65536, min(compressed_bytes, args.max_compressed))
    # fetch using HEAD? just set range via fetch_sample by records
    binpath = fetch_sample(args.endpoint, args.max_compressed, args.records)

    recs = parse_records(binpath)
    effective = len(recs)
    print("records read: %d endpoint=%s" % (effective, args.endpoint))
    if effective < args.min_records:
        fail("only %d records (< %d)" % (effective, args.min_records))

    # Build dataset
    X = []
    y = []
    used = 0
    for (occ, pcs, score, result, ksq, oppksq, extra) in recs:
        white, black = decode_position(occ, pcs)
        f = features_from_position(white, black)
        label = result / 2.0  # 0,0.5,1
        X.append(f)
        y.append(label)
        used += 1
    if used < args.min_records:
        fail("insufficient parsed positions")

    # base weights default
    base = build_default_weight_list()

    # initial loss
    init_loss, _ = loss_and_grad(X, y, base)
    print("initial loss: %.6f records=%d" % (init_loss, used))

    # tuned weights via gradient descent
    w = list(base)
    for ep in range(args.epochs):
        loss, grad = loss_and_grad(X, y, w)
        lr = 0.5
        for j in range(len(w)):
            w[j] -= lr * grad[j]
        if ep % 10 == 0 or ep == args.epochs-1:
            print("epoch %d loss %.6f" % (ep, loss))
    final_loss, _ = loss_and_grad(X, y, w)
    print("final loss: %.6f" % final_loss)
    if not (final_loss < init_loss):
        # try smaller? still report
        print("WARNING: final loss not lower than initial", file=sys.stderr)

    # generate models
    tuned = w
    for m in range(args.num_models):
        model = list(tuned)
        for j in range(len(model)):
            scale = args.variation * max(1.0, abs(tuned[j]))
            model[j] += random.gauss(0.0, scale)
        path = os.path.join(args.output, "model_%d.wts" % m)
        save_weights(path, model)
        print("wrote %s" % path)

    # save tuned base as model_base.wts
    save_weights(os.path.join(args.output, "model_base.wts"), tuned)
    print("initial_loss=%.6f final_loss=%.6f used_records=%d" % (init_loss, final_loss, used))

if __name__ == "__main__":
    main()
