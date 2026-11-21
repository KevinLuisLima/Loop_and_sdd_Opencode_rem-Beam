import os, subprocess, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def test_tournament_runs():
    weights = os.path.join(ROOT, "weights")
    r = subprocess.run([sys.executable, os.path.join(ROOT, "scripts", "tournament.py"),
                        "--models-dir", weights, "--generations", "1", "--games", "1",
                        "--depth", "2", "--time", "50", "--judge-depth", "3"],
                       capture_output=True, text=True, timeout=300, cwd=ROOT)
    assert r.returncode == 0, r.stderr + r.stdout
    assert "winrate=" in r.stdout
    assert "best_model=" in r.stdout
if __name__ == "__main__":
    test_tournament_runs()
    print("tournament tests ok")
