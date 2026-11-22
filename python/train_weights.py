#!/usr/bin/env python3
import argparse
import os
import random
import struct
import subprocess
import sys
import tempfile
import urllib.request

URL = "https://huggingface.co/datasets/linrock/bullet-training-data/resolve/main/S1/UHO.pdist.iter-1.bullet.bin.zst"
RECORD = 32
FEATURES = 392
MAGIC = b"CHWT"

def fetch_raw(sample_bytes):
    req = urllib.request.Request(URL, headers={"Range": "bytes=0-%d" % (sample_bytes - 1)})
    with urllib.request.urlopen(req, timeout=60) as r:
        data = r.read()
    tmp = tempfile.NamedTemporaryFile(delete=False, suffix=".zst")
    tmp.write(data)
    tmp.close()
    proc = subprocess.run(["zstd", "-dc", tmp.name], capture_output=True)
    os.unlink(tmp.name)
    raw = proc.stdout
    if len(raw) < RECORD * 100:
        raise RuntimeError("not enough decompressed data: %d bytes" % len(raw))
    return raw

def decode_record(rec):
    occ, pcs, score, result, ksq, opp_ksq, extra = struct.unpack("<Q16s hBBB3s", rec)
    pieces = []
    occ_tmp = occ
    idx = 0
    while occ_tmp:
        lsb = occ_tmp & -occ_tmp
        sq = lsb.bit_length() - 1
        nibble = (pcs[idx // 2] >> (4 * (idx % 2))) & 0xF
        pieces.append((sq, nibble))
        occ_tmp ^= lsb
        idx += 1
    board = [None] * 64
    chars = "pnbrqk"
    for sq, nib in pieces:
        typ = nib & 7
        col = (nib >> 3) & 1
        ch = chars[typ]
        board[sq] = ch.upper() if col == 0 else ch
    rows = []
    for r in range(7, -1, -1):
        s = ""
        empty = 0
        for f in range(8):
            sq = r * 8 + f
            if board[sq] is None:
                empty += 1
            else:
                if empty:
                    s += str(empty)
                    empty = 0
                s += board[sq]
        if empty:
            s += str(empty)
        rows.append(s)
    fen = "/".join(rows) + " w - - 0 1"

    feat = [0.0] * FEATURES
    counts = [[0] * 8, [0] * 8]
    for sq, nib in pieces:
        typ = nib & 7
        col = (nib >> 3) & 1
        if typ <= 4:
            if col == 0:
                feat[typ] += 1.0
            else:
                feat[typ] -= 1.0
        base = 5 + typ * 64
        if col == 0:
            feat[base + sq] += 1.0
        else:
            feat[base + (sq ^ 56)] -= 1.0
        if typ == 0:
            counts[col][sq % 8] += 1
    dw = sum(c - 1 for c in counts[0] if c > 1)
    db = sum(c - 1 for c in counts[1] if c > 1)
    iw = 0
    ib = 0
    for sq, nib in pieces:
        typ = nib & 7
        col = (nib >> 3) & 1
        if typ == 0:
            f = sq % 8
            if col == 0:
                left = counts[0][f - 1] if f > 0 else 0
                right = counts[0][f + 1] if f < 7 else 0
                if left == 0 and right == 0:
                    iw += 1
            else:
                left = counts[1][f - 1] if f > 0 else 0
                right = counts[1][f + 1] if f < 7 else 0
                if left == 0 and right == 0:
                    ib += 1
    feat[389] += float(dw - db)
    feat[390] += float(iw - ib)
    target = result / 2.0
    return fen, feat, target, score

def sigmoid(x):
    if x >= 0:
        z = 1.0 / (1.0 + __import__("math").exp(-x))
    else:
        e = __import__("math").exp(x)
        z = e / (1.0 + e)
    return z

def dot(a, b):
    s = 0.0
    for i in range(len(a)):
        s += a[i] * b[i]
    return s

def make_loss(samples, weights, scale):
    import math
    loss = 0.0
    n = len(samples)
    for feat, y in samples:
        e = dot(feat, weights)
        p = sigmoid(e / scale)
        eps = 1e-6
        p = min(max(p, eps), 1 - eps)
        loss -= y * math.log(p) + (1 - y) * math.log(1 - p)
    return loss / n

def train(samples, epochs, lr, scale, seed):
    random.seed(seed)
    weights = [0.0] * FEATURES
    for i, v in enumerate([100.0, 320.0, 330.0, 500.0, 900.0]):
        weights[i] = v
    loss0 = make_loss(samples, weights, scale)
    for _ in range(epochs):
        grad = [0.0] * FEATURES
        for feat, y in samples:
            e = dot(feat, weights)
            p = sigmoid(e / scale)
            diff = p - y
            for j in range(FEATURES):
                grad[j] += diff * feat[j]
        for j in range(FEATURES):
            weights[j] -= (lr / len(samples)) * grad[j]
    loss1 = make_loss(samples, weights, scale)
    return weights, loss0, loss1

def write_weights(path, weights):
    with open(path, "wb") as f:
        f.write(MAGIC)
        f.write(struct.pack("<III", 1, len(weights), 0))
        f.write(struct.pack("<%df" % len(weights), *weights))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--num-models", type=int, default=4)
    ap.add_argument("--variation", type=float, default=0.15)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--output-dir", required=True)
    ap.add_argument("--sample-size", type=int, default=1200)
    ap.add_argument("--epochs", type=int, default=60)
    ap.add_argument("--learning-rate", type=float, default=2.0)
    ap.add_argument("--sample-bytes", type=int, default=4000000)
    args = ap.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)
    raw = fetch_raw(args.sample_bytes)
    recs = [raw[i:i + RECORD] for i in range(0, len(raw) - RECORD, RECORD)]
    print("records parsed:", len(recs), file=sys.stderr)
    rng = random.Random(args.seed)
    if len(recs) < args.sample_size:
        sample_recs = recs
    else:
        sample_recs = rng.sample(recs, args.sample_size)
    samples = []
    for rec in sample_recs:
        try:
            fen, feat, target, score = decode_record(rec)
        except Exception:
            continue
        samples.append((feat, target))
    print("training samples:", len(samples), file=sys.stderr)

    weights, loss0, loss1 = train(samples, args.epochs, args.learning_rate, 400.0, args.seed)
    print("loss before:", loss0, file=sys.stderr)
    print("loss after:", loss1, file=sys.stderr)
    if not (loss1 < loss0):
        print("WARNING loss did not decrease", file=sys.stderr)

    base_path = os.path.join(args.output_dir, "model_base.bin")
    write_weights(base_path, weights)

    for i in range(args.num_models):
        r = random.Random(args.seed + (i + 1) * 997)
        var = [w * (1.0 + args.variation * (2 * r.random() - 1.0)) for w in weights]
        # add small per-weight spread
        for j in range(len(var)):
            var[j] += args.variation * 5.0 * (r.random() - 0.5)
        out = os.path.join(args.output_dir, "model_%d.bin" % i)
        write_weights(out, var)
    # store report
    with open(os.path.join(args.output_dir, "training_report.txt"), "w") as f:
        f.write("records=%d samples=%d loss_before=%.6f loss_after=%.6f\n" % (len(recs), len(samples), loss0, loss1))

if __name__ == "__main__":
    main()
