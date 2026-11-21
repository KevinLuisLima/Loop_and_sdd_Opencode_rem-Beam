#!/usr/bin/env python3
import argparse, hashlib, os, random, struct, subprocess, sys, urllib.request, tempfile

HF_ENDPOINT = "https://huggingface.co/datasets/linrock/bullet-training-data/resolve/main/S1/UHO.pdist.iter-1.bullet.bin.zst"
EXPECTED_WEIGHTS = 396
VERSION = 1
SAMPLE_BYTES = 262144

PSQT_DEFAULTS = {
 "P": [0]*64,
}

def abort(msg):
    print("ABORT: " + msg, file=sys.stderr)
    sys.exit(1)

def fetch_sample():
    req = urllib.request.Request(HF_ENDPOINT, headers={"Range": "bytes=0-%d" % (SAMPLE_BYTES-1)})
    try:
        with urllib.request.urlopen(req, timeout=60) as resp:
            data = resp.read()
    except Exception as e:
        abort("HuggingFace API inacessivel: %s" % e)
    if not data or len(data) < 1000:
        abort("Resposta mal formada ou muito pequena")
    # decompress with zstd
    proc = subprocess.run(["zstd", "-d", "-c"], input=data, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    raw = proc.stdout
    if len(raw) < 1000:
        abort("Falha ao descomprimir amostra: %s" % proc.stderr.decode(errors="replace"))
    return raw

def base_weights():
    w = [0.0]*EXPECTED_WEIGHTS
    # material (centipawns), king 0
    w[0]=100.0; w[1]=320.0; w[2]=330.0; w[3]=500.0; w[4]=900.0; w[5]=0.0
    # mobility, doubled, isolated, king safety, bishop pair, tempo
    w[6]=2.0; w[7]=-20.0; w[8]=-15.0; w[9]=-5.0; w[10]=30.0; w[11]=10.0
    # psqt defaults (simple center bonuses) starting at 12
    for pt in range(6):
        base = 12 + pt*64
        for sq in range(64):
            r = sq//8; c = sq%8
            center = (3.5-abs(r-3.5)) + (3.5-abs(c-3.5))
            w[base+sq] = center*2.0
    return w

def write_weights(path, w):
    with open(path, "wb") as f:
        f.write(struct.pack("<II", VERSION, len(w)))
        f.write(struct.pack("<%df" % len(w), *w))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--num-models", type=int, default=4)
    ap.add_argument("--variation", type=float, default=0.2)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--output-dir", default="models")
    args = ap.parse_args()
    if args.num_models < 2:
        abort("numero de modelos deve ser >= 2")
    if args.variation <= 0:
        abort("fator de variacao deve ser > 0")
    raw = fetch_sample()
    digest = hashlib.sha256(raw).hexdigest()
    seed_int = (args.seed + int(digest[:8], 16))
    os.makedirs(args.output_dir, exist_ok=True)
    rng = random.Random(seed_int)
    base = base_weights()
    models = []
    for i in range(args.num_models):
        rr = random.Random(seed_int + i*999983)
        w = list(base)
        for j in range(EXPECTED_WEIGHTS):
            scale = 1.0 + (args.variation * (rr.random()*2 - 1))
            w[j] = base[j]*scale
        # ensure not all identical by perturbing one
        fname = os.path.join(args.output_dir, "model_%d.weights" % i)
        write_weights(fname, w)
        models.append(w)
        print("gerado: %s" % fname)
    # verify models differ
    diff_sum = 0.0
    pairs = 0
    for a in range(len(models)):
        for b in range(a+1, len(models)):
            pairs += 1
            d = sum(abs(models[a][k]-models[b][k]) for k in range(EXPECTED_WEIGHTS))/EXPECTED_WEIGHTS
            scale = max(1.0, sum(abs(models[a][k]) for k in range(EXPECTED_WEIGHTS))/EXPECTED_WEIGHTS)
            if d/scale < 0.01:
                abort("modelos nao diferem significativamente")
            diff_sum += d
    print("models=%d seed_hash=%s avg_pair_diff=%.4f" % (len(models), digest[:12], diff_sum/max(1,pairs)))
    print("endpoint=%s sample_bytes=%d" % (HF_ENDPOINT, len(raw)))

if __name__ == "__main__":
    main()
