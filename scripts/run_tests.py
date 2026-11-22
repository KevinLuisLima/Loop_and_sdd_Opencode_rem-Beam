import subprocess, os, sys

ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILDS=os.path.join(ROOT,"build")
ENGINE=os.path.join(BUILDS,"engine")
failed=0; total=0

def run(cmd):
    global failed,total
    total+=1
    try:
        r=subprocess.run(cmd, capture_output=True, text=True, timeout=600)
    except Exception as e:
        print("EXCEPTION", e); failed+=1; return False
    if r.returncode!=0:
        print("FAIL cmd:", " ".join(cmd)); print(r.stdout); print(r.stderr); failed+=1; return False
    print(r.stdout.strip().splitlines()[-1] if r.stdout.strip() else "PASS")
    return True

# unit tests
for f in sorted(os.listdir(BUILDS)):
    if f.startswith("test_") and os.path.isfile(os.path.join(BUILDS,f)):
        run([os.path.join(BUILDS,f)])

# sanitizer smoke: run engine_san
san=os.path.join(BUILDS,"engine_san")
if os.path.exists(san):
    run([san, "--fen","rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1","--time","200","--depth","6"])
else:
    print("sanitizer binary missing"); failed+=1; total+=1

# match tests (random/greedy/tactical)
mt=os.path.join(ROOT,"tests","test_matches.py")
run([sys.executable, mt])

# python weights generator smoke (should not fail)
gen=os.path.join(ROOT,"scripts","gen_weights.py")
run([sys.executable, gen, "--num-models","2","--records","600","--min-records","200","--epochs","5","--output", os.path.join("/tmp","opencode","smokemodels")])

# tournament smoke
tourn=os.path.join(ROOT,"scripts","tournament.py")
run([sys.executable, tourn, "--generations","1","--games-per-pair","1","--time-ms","20","--depth","4","--judge-time-ms","20","--judge-depth","4"])

print("%d/%d testes passaram" % (total-failed, total))
sys.exit(1 if failed else 0)
