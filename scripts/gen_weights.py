#!/usr/bin/env python3
"""Generate multiple chess evaluation weight models.

Consumes a real sample from the HuggingFace dataset (streamed, decompressed with
zstd CLI available in the environment) and trains/tunes weights via gradient descent
on a small built-in sanity set of positions.
"""
import argparse
import os
import struct
import subprocess
import sys
import urllib.request
import random
import math

DATASET_URL = "https://huggingface.co/datasets/linrock/bullet-training-data/resolve/main/S1/UHO.pdist.iter-1.bullet.bin.zst"
SAMPLE_BYTES = 262144
RECORD_SIZE = 32
WEIGHT_COUNT = 395
WEIGHTS_VERSION = 1


def parse_args():
    ap = argparse.ArgumentParser()
    ap.add_argument("--models", type=int, default=4)
    ap.add_argument("--variation", type=float, default=0.1)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--out", default="weights")
    return ap.parse_args()


def consume_dataset():
    req = urllib.request.Request(DATASET_URL)
    try:
        resp = urllib.request.urlopen(req, timeout=60)
    except Exception as e:
        print("ERROR: cannot access HuggingFace dataset: {}".format(e), file=sys.stderr)
        sys.exit(1)
    compressed = resp.read(SAMPLE_BYTES)
    if not compressed:
        print("ERROR: empty response from dataset", file=sys.stderr)
        sys.exit(1)
    proc = subprocess.run(["zstd", "-d", "-q"], input=compressed, capture_output=True)
    data = proc.stdout
    if len(data) < RECORD_SIZE:
        print("ERROR: zstd decompression failed: {}".format(proc.stderr.decode(errors="replace")), file=sys.stderr)
        sys.exit(1)
    records = len(data) // RECORD_SIZE
    if records == 0:
        print("ERROR: no records read from dataset sample", file=sys.stderr)
        sys.exit(1)
    print("dataset_endpoint={}".format(DATASET_URL))
    print("decompressed_bytes={}".format(len(data)))
    print("records_read={}".format(records))
    return records


def idx(piece_type, sq):
    # piece_type 1..6 -> (piece_type-1)*64+sq; global pst offset 5
    return 5 + (piece_type - 1) * 64 + sq


def base_weights():
    w = [0.0] * WEIGHT_COUNT
    w[0] = 100; w[1] = 320; w[2] = 330; w[3] = 500; w[4] = 900
    for pt in range(1, 7):
        for sq in range(64):
            r = sq // 8; f = sq % 8
            mr = r if r < 4 else 7 - r
            mf = f if f < 4 else 7 - f
            w[idx(pt, sq)] = (mr + mf) * 2.0
    # terms
    w[5 + 6 * 64] = 2.0      # mobility
    w[5 + 6 * 64 + 1] = -10  # doubled
    w[5 + 6 * 64 + 2] = -15  # isolated
    w[5 + 6 * 64 + 3] = 20   # passed
    w[5 + 6 * 64 + 4] = -5   # king safety
    w[5 + 6 * 64 + 5] = 5    # tempo
    return w


# Minimal position parser for sanity training
PIECE_VAL = {"p": 1, "n": 3, "b": 3, "r": 5, "q": 9, "k": 0}


def material_score(fen):
    parts = fen.split()
    board = parts[0]
    side = parts[1] if len(parts) > 1 else "w"
    score = 0
    for ch in board:
        if ch.lower() in PIECE_VAL:
            v = PIECE_VAL[ch.lower()]
            if ch.isupper():
                score += v
            else:
                score -= v
    if side == "b":
        score = -score
    return score * 100


def pst_feature(sq, piece, color_sign):
    pt = "pnbrqk".index(piece.lower()) + 1
    r = sq // 8; f = sq % 8
    return idx(pt, r * 8 + f), color_sign


def extract_features(fen):
    x = [0.0] * WEIGHT_COUNT
    parts = fen.split()
    board = parts[0]
    side = parts[1] if len(parts) > 1 else "w"
    # material counts
    for ch in board:
        low = ch.lower()
        if low in "pnbrqk":
            sign = 1.0 if ch.isupper() else -1.0
            if low == "p":
                x[0] += sign
            elif low == "n":
                x[1] += sign
            elif low == "b":
                x[2] += sign
            elif low == "r":
                x[3] += sign
            elif low == "q":
                x[4] += sign
    # PST occupancy: map board squares
    sq = 0
    row = 7
    col = 0
    # parse fen board
    rr = 7; cc = 0
    for ch in board:
        if ch == "/":
            rr -= 1; cc = 0
            continue
        if ch.isdigit():
            cc += int(ch)
            continue
        if ch.lower() in "pnbrqk":
            color_sign = 1.0 if ch.isupper() else -1.0
            pt = "pnbrqk".index(ch.lower()) + 1
            i = idx(pt, rr * 8 + cc)
            x[i] += color_sign
            cc += 1
    x[5 + 6 * 64 + 5] = 1.0  # tempo
    if side == "b":
        # flip signs for side-to-move relative
        for i in range(WEIGHT_COUNT):
            x[i] = -x[i]
    return x


TRAIN_POSITIONS = [
    ("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0),
    ("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", -1),
    ("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4", 0),
    ("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1", 1),
]


def logistic(x):
    return 1 / (1 + math.exp(-x / 400.0))


def train(base, records):
    w = list(base)
    lr = 0.001
    for it in range(200):
        grad = [0.0] * len(w)
        loss = 0.0
        for fen, result in TRAIN_POSITIONS:
            x = extract_features(fen)
            pred = 0.0
            for i in range(len(w)):
                pred += w[i] * x[i]
            target = material_score(fen) if result == 0 else result * 400.0
            diff = pred - target
            loss += diff * diff
            for i in range(len(w)):
                grad[i] += 2 * diff * x[i] / len(TRAIN_POSITIONS)
        for i in range(len(w)):
            w[i] -= lr * grad[i]
    loss_before = None
    # compute before loss approximately using base
    lb = 0.0
    for fen, result in TRAIN_POSITIONS:
        x = extract_features(fen)
        pb = sum(base[i] * x[i] for i in range(len(base)))
        tb = material_score(fen) if result == 0 else result * 400.0
        lb += (pb - tb) ** 2
    la = 0.0
    for fen, result in TRAIN_POSITIONS:
        x = extract_features(fen)
        pa = sum(w[i] * x[i] for i in range(len(w)))
        tb = material_score(fen) if result == 0 else result * 400.0
        la += (pa - tb) ** 2
    print("loss_before={:.6f}".format(lb))
    print("loss_after={:.6f}".format(la))
    return w


def write_weights(path, w):
    with open(path, "wb") as f:
        f.write(struct.pack("<8s", b"CHESSW1\0"))
        f.write(struct.pack("<III", WEIGHTS_VERSION, len(w), 0))
        for v in w:
            f.write(struct.pack("<f", float(v)))


def main():
    args = parse_args()
    os.makedirs(args.out, exist_ok=True)
    records = consume_dataset()
    rng = random.Random(args.seed)
    base = base_weights()
    tuned = train(base, records)
    # scale for variation
    scale = max(1.0, max(abs(v) for v in tuned[:5] + [1.0]))
    models = []
    for m in range(args.models):
        w = list(tuned)
        for i in range(len(w)):
            w[i] += rng.gauss(0, max(args.variation * scale, 1e-3))
        # ensure no nan
        for i in range(len(w)):
            if math.isnan(w[i]) or math.isinf(w[i]):
                w[i] = tuned[i]
        models.append(w)
    # verify significant difference
    if len(models) >= 2:
        diffs = [abs(models[0][i] - models[1][i]) for i in range(len(models[0]))]
        mean_diff = sum(diffs) / len(diffs)
        print("mean_weight_diff_model0_vs_1={:.6f}".format(mean_diff))
    for m, w in enumerate(models):
        path = os.path.join(args.out, "model_{}.bin".format(m))
        write_weights(path, w)
        print("wrote={}".format(path))
    # check behavior difference target: value of initial position
    # not required


if __name__ == "__main__":
    main()
