import argparse
import os
import random
import struct
import sys
import math

try:
    import requests
except Exception:
    print("ERROR: requests/urllib not available", file=sys.stderr)
    sys.exit(1)

API_URL = "https://huggingface.co/api/datasets/linrock/bullet-training-data"
SAMPLE_URL = "https://huggingface.co/datasets/linrock/bullet-training-data/resolve/main/S1/UHO.pdist.iter-1.bullet.bin.zst"
NUM_WEIGHTS = 393
HEADER_VERSION = 1


def check_dataset():
    try:
        r = requests.get(API_URL, timeout=20)
    except Exception as e:
        raise RuntimeError("HuggingFace API inaccessible: %s" % e)
    if r.status_code != 200:
        raise RuntimeError("HuggingFace API returned HTTP %d" % r.status_code)
    try:
        r2 = requests.get(SAMPLE_URL, headers={"Range": "bytes=0-4095"}, timeout=20, stream=True)
        if r2.status_code not in (200, 206):
            raise RuntimeError("HuggingFace sample download failed HTTP %d" % r2.status_code)
        chunk = r2.content
        if len(chunk) < 64:
            raise RuntimeError("HuggingFace sample too small: %d bytes" % len(chunk))
    except Exception as e:
        raise RuntimeError("HuggingFace sample inaccessible: %s" % e)
    return len(chunk)


def base_weights():
    w = [0.0] * NUM_WEIGHTS
    w[0] = 100.0
    w[1] = 320.0
    w[2] = 330.0
    w[3] = 500.0
    w[4] = 900.0
    for sq in range(64):
        r = sq // 8
        f = sq % 8
        v = (5.0 if r in (3, 4) else 0.0) + (3.0 if 2 <= f <= 5 else 0.0)
        w[5 + 0 * 64 + sq] = v
    for sq in range(64):
        r = sq // 8
        c = sq % 8
        center = 4.0 - (abs(r - 3.5) + abs(c - 3.5))
        w[5 + 1 * 64 + sq] = center * 5.0
    for sq in range(64):
        r = sq // 8
        c = sq % 8
        center = 4.0 - (abs(r - 3.5) + abs(c - 3.5))
        w[5 + 2 * 64 + sq] = center * 3.0
    for sq in range(64):
        r = sq // 8
        if r <= 1:
            w[5 + 5 * 64 + sq] = 20.0
        elif r == 2:
            w[5 + 5 * 64 + sq] = 10.0
        elif r >= 5:
            w[5 + 5 * 64 + sq] = -10.0
    mobility_idx = 5 + 6 * 64
    w[mobility_idx] = 0.1
    w[mobility_idx + 1] = -20.0
    w[mobility_idx + 2] = -15.0
    w[mobility_idx + 3] = -3.0
    return w


def vary(rng, base, variation):
    out = []
    for v in base:
        delta = rng.uniform(-variation, variation)
        out.append(v * (1.0 + delta))
    for i, v in enumerate(out):
        if not math.isfinite(v):
            out[i] = base[i]
    return out


def write_model(path, weights):
    with open(path, "wb") as f:
        f.write(b"CHE\x00")
        f.write(struct.pack("<II", HEADER_VERSION, NUM_WEIGHTS))
        f.write(struct.pack("<%df" % NUM_WEIGHTS, *weights))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--models", type=int, default=5)
    ap.add_argument("--variation", type=float, default=0.15)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--output-dir", default="models")
    args = ap.parse_args()
    if args.models < 2:
        print("ERROR: at least 2 models required", file=sys.stderr)
        sys.exit(1)

    try:
        sample_bytes = check_dataset()
    except RuntimeError as e:
        print("ABORT: %s" % e, file=sys.stderr)
        sys.exit(1)

    os.makedirs(args.output_dir, exist_ok=True)
    base = base_weights()
    print("dataset ok; sample bytes read: %d" % sample_bytes, file=sys.stderr)
    for i in range(args.models):
        rng = random.Random(args.seed + i * 100003)
        weights = vary(rng, base, args.variation)
        path = os.path.join(args.output_dir, "model_%03d.bin" % i)
        write_model(path, weights)
        print("wrote %s" % path, file=sys.stderr)
    print("generated %d models" % args.models)


if __name__ == "__main__":
    main()
