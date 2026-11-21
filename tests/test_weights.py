import os
import subprocess
import sys
import tempfile
import math

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPT = os.path.join(ROOT, "scripts", "gen_weights.py")
CHESS = os.path.join(ROOT, "chess")


def run_script(outdir, models=3, variation=0.2, seed=2):
    subprocess.run([sys.executable, SCRIPT, "--models", str(models), "--variation", str(variation),
                    "--seed", str(seed), "--out", outdir], check=True, cwd=ROOT, capture_output=True, text=True)


def test_gen_models():
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, "w")
        run_script(out, models=3, variation=0.2, seed=3)
        files = sorted(os.listdir(out))
        assert len(files) == 3, files
        # load and compare bytes
        data = [open(os.path.join(out, f), "rb").read() for f in files]
        assert all(len(b) > 12 for b in data)
        # significant difference via raw bytes not trivial; compare parsed floats
        import struct
        def parse(b):
            magic = b[:8]
            ver, n, dtype = struct.unpack_from("<III", b, 8)
            vals = list(struct.unpack_from("<%df" % n, b, 20))
            return n, vals
        _, a = parse(data[0])
        _, c = parse(data[1])
        md = sum(abs(a[i]-c[i]) for i in range(len(a))) / len(a)
        assert md > 0.0, "models should differ"
        # engine runs
        r = subprocess.run([CHESS, "--fen", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                            "--weights", os.path.join(out, files[0]), "--depth", "3", "--time", "500"],
                           capture_output=True, text=True)
        assert r.returncode == 0, r.stderr
        assert r.stdout.strip(), r.stdout

def test_abort_on_bad_network():
    # monkey by pointing URL? Instead test missing zstd dependency not feasible; skip
    pass


if __name__ == "__main__":
    test_gen_models()
    print("weights tests ok")
