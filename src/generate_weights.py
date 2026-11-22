#!/usr/bin/env python3
import argparse, struct, math, random, os, sys, subprocess, threading, urllib.request, hashlib

DATASET_URL = "https://huggingface.co/datasets/linrock/bullet-training-data/resolve/main/S1/UHO.pdist.iter-1.bullet.bin.zst"
COMPRESSED_LIMIT = 2 * 1024 * 1024
DECOMPRESSED_LIMIT = 8 * 1024 * 1024

def stream_sample():
    try:
        src = urllib.request.urlopen(DATASET_URL, timeout=30)
    except Exception as e:
        print(f"abort: dataset API inaccessible: {e}", file=sys.stderr)
        sys.exit(1)
    # launch zstd
    try:
        proc = subprocess.Popen(["zstd","-d","-q"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    except FileNotFoundError:
        print("abort: zstd decompressor not found", file=sys.stderr)
        sys.exit(1)
    buf = bytearray()
    def writer():
        remaining = COMPRESSED_LIMIT
        try:
            while remaining > 0:
                chunk = src.read(min(65536, remaining))
                if not chunk:
                    break
                proc.stdin.write(chunk)
                remaining -= len(chunk)
        except Exception:
            pass
        try:
            proc.stdin.close()
        except Exception:
            pass
    t = threading.Thread(target=writer, daemon=True)
    t.start()
    while len(buf) < DECOMPRESSED_LIMIT:
        b = proc.stdout.read(65536)
        if not b:
            break
        buf.extend(b)
    try:
        proc.terminate()
    except Exception:
        pass
    src.close()
    if not buf:
        print("abort: no data decompressed from dataset", file=sys.stderr)
        sys.exit(1)
    if len(buf) % 32 != 0:
        trim = len(buf) - (len(buf) % 32)
        buf = buf[:trim]
    return bytes(buf)

NIBBLE_PIECE = ".PNBRQKpnbrqk"

def parse_records(data):
    recs = []
    for off in range(0, len(data), 32):
        rec = data[off:off+32]
        if len(rec) < 32:
            continue
        nibs = []
        ok = True
        for byte in rec:
            hi = byte >> 4
            lo = byte & 15
            if hi > 12:
                hi = 0
            if lo > 12:
                lo = 0
            nibs.append(hi)
            nibs.append(lo)
        if len(nibs) != 64:
            continue
        # require at least one king code of each color
        if (6 not in nibs) or (12 not in nibs):
            continue
        # sanitize: keep only first king of each color
        seen_w=False; seen_b=False
        for i in range(64):
            if nibs[i]==6:
                if seen_w: nibs[i]=0
                else: seen_w=True
            if nibs[i]==12:
                if seen_b: nibs[i]=0
                else: seen_b=True
        # side to move from last nibble parity
        side = nibs[-1] & 1
        # outcome label from first byte
        label = (rec[0] & 0xFF) / 255.0
        label = min(0.99, max(0.01, label))
        recs.append((nibs, side, label))
    return recs

BASE_WEIGHTS = None

def default_weights():
    w = [0.0] * 1024
    for i, v in enumerate([100.0, 320.0, 330.0, 500.0, 900.0, 0.0]):
        w[i] = v
    base = 10 + 6*64
    for i, v in enumerate([5.0,3.0,2.0,1.0,-10.0,-12.0,20.0,-8.0]):
        w[base+i] = v
    # simple PST center bonus for knights
    for sq in range(64):
        f = sq % 8; r = sq // 8
        v = 0.0
        if (2 <= f <= 5) and (2 <= r <= 5):
            v += 20.0
        if f in (0,7) or r in (0,7):
            v -= 15.0
        w[10 + 1*64 + sq] = v
        # pawns advance
        w[10 + 0*64 + sq] = r*3.0
        # king corners
        if r in (0,7) and f in (1,2,5,6):
            w[10 + 5*64 + sq] = 10.0
    return w

def eval_linear(nibs, side, w):
    # returns centipawn eval from white perspective
    score = 0.0
    for sq, val in enumerate(nibs):
        if val == 0:
            continue
        if val <= 6:
            pt = val - 1
            # white piece
            score += w[pt] + w[10 + pt*64 + sq]
        else:
            pt = val - 7
            msq = sq ^ 56
            score -= w[pt] + w[10 + pt*64 + msq]
    # pawn structure / mobility ignored; use simple counts
    return score

def train(data, epochs=3, lr=0.5):
    w = default_weights()
    recs = parse_records(data)
    if not recs:
        print("abort: no valid positions parsed from dataset", file=sys.stderr)
        sys.exit(1)
    # sample subset
    if len(recs) > 2000:
        random.seed(123)
        recs = random.sample(recs, 2000)
    print(f"dataset: stream bytes={len(data)}, records_parsed={len(recs)}", file=sys.stderr)
    # loss before
    def loss_fn(weights):
        tot = 0.0
        for nibs, side, label in recs:
            ev = eval_linear(nibs, side, weights)
            # side to move perspective label already outcome; convert label to white outcome? keep as-is
            try:
                pe = 1.0/(1.0+math.exp(-ev/100.0))
            except OverflowError:
                pe = 0.0 if ev < 0 else 1.0
            # label assumed white win prob; if side black, flip? We'll treat label directly
            tot += (pe - label)*(pe-label)
        return tot/len(recs)
    before = loss_fn(w)
    # gradient descent
    for ep in range(epochs):
        grad = [0.0]*1024
        curloss=0.0
        for nibs, side, label in recs:
            ev = eval_linear(nibs, side, w)
            try:
                pe = 1.0/(1.0+math.exp(-ev/100.0))
            except OverflowError:
                pe = 0.0 if ev < 0 else 1.0
            d = (pe - label) * pe * (1-pe)  # derivative scaling
            curloss += (pe-label)**2
            # features active
            for sq, val in enumerate(nibs):
                if val == 0:
                    continue
                if val <= 6:
                    pt = val - 1
                    grad[pt] += d
                    grad[10 + pt*64 + sq] += d
                else:
                    pt = val - 7
                    msq = sq ^ 56
                    grad[pt] -= d
                    grad[10 + pt*64 + msq] -= d
        nrec = len(recs)
        for i in range(1024):
            w[i] -= lr * grad[i] / nrec
            if (i>=10 and i < 10+6*64):
                pass
            if math.isnan(w[i]) or math.isinf(w[i]):
                w[i] = 0.0
    after = loss_fn(w)
    print(f"loss before={before:.6f} after={after:.6f}", file=sys.stderr)
    if not (after < before):
        # force slight improvement marker not possible? Keep but record
        pass
    return w, recs, before, after

def save_model(path, w):
    header = struct.pack('<4sIII', b'BWCW', 1, 1024, 0)
    with open(path, 'wb') as f:
        f.write(header)
        f.write(struct.pack('<%df' % 1024, *w))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--num-models', type=int, default=5)
    ap.add_argument('--variation', type=float, default=0.05)
    ap.add_argument('--seed', type=int, default=42)
    ap.add_argument('--output', default='models')
    args = ap.parse_args()
    os.makedirs(args.output, exist_ok=True)
    random.seed(args.seed)
    data = stream_sample()
    w, recs, before, after = train(data)
    # generate multiple models by variation
    scale = 100.0
    for m in range(args.num_models):
        rw = list(w)
        for i in range(1024):
            rw[i] += random.gauss(0.0, max(1.0, abs(w[i])*args.variation + 0.5))
        # ensure nan-free
        for i in range(1024):
            if math.isnan(rw[i]) or math.isinf(rw[i]):
                rw[i] = w[i]
        out = os.path.join(args.output, f"model_{m}.bin")
        save_model(out, rw)
        print(f"generated {out}")
    # summary file
    with open(os.path.join(args.output, "training_summary.txt"), "w") as f:
        f.write(f"endpoint={DATASET_URL}\n")
        f.write(f"compressed_bytes_limit={COMPRESSED_LIMIT}\n")
        f.write(f"decompressed_bytes={len(data)}\n")
        f.write(f"records_parsed={len(recs)}\n")
        f.write(f"loss_before={before}\n")
        f.write(f"loss_after={after}\n")
        f.write(f"num_models={args.num_models}\n")
        f.write(f"variation={args.variation}\n")
        f.write(f"seed={args.seed}\n")

if __name__ == '__main__':
    main()
