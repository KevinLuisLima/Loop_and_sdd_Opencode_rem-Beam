#include "board.h"
#include "movegen.h"
#include "eval.h"
#include "search.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
static int total = 0;

static void check(const char *name, int cond) {
    total++;
    if (cond) {
        /* silent */ 
    } else {
        failures++;
        fprintf(stderr, "FAIL: %s\n", name);
    }
}

static unsigned long long perft(Position *p, int depth) {
    int moves[512];
    int n = movegen_legal_moves(p, moves, 512);
    if (depth == 1) return (unsigned long long)n;
    if (n == 0) return 1;
    unsigned long long nodes = 0;
    for (int i = 0; i < n; i++) {
        int m = moves[i];
        int from = m & 63, to = (m >> 6)&63, promo = (m>>12)&7;
        Undo u;
        pos_make_move(p, from, to, promo, &u);
        nodes += perft(p, depth - 1);
        pos_unmake_move(p, &u);
    }
    return nodes;
}

int main(void) {
    /* pos from fen: startpos */
    Position p;
    pos_init_startpos(&p);
    check("startpos has pieces", p.bb[0][PT_KING] != 0);
    /* legal moves count */
    int moves[512];
    int n = movegen_legal_moves(&p, moves, 512);
    check("startpos 20 moves", n == 20);

    /* perft 1 */
    Position p1 = p;
    unsigned long long r = perft(&p1, 1);
    check("perft1=20", r == 20ULL);
    /* perft 2 */
    Position p2 = p;
    r = perft(&p2, 2);
    check("perft2=400", r == 400ULL);
    /* perft 3 */
    Position p3 = p;
    r = perft(&p3, 3);
    check("perft3=8902", r == 8902ULL);
    /* perft 4 */
    Position p4 = p;
    r = perft(&p4, 4);
    check("perft4=197281", r == 197281ULL);
    /* perft 5 */
    Position p5 = p;
    r = perft(&p5, 5);
    check("perft5=4865609", r == 4865609ULL);

    /* Kiwipete */
    const char *kiwi = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
    Position kp;
    check("kiwi fen parse", pos_from_fen(&kp, kiwi) == 0);
    unsigned long long pk = perft(&kp, 4);
    check("kiwi perft4=4085603", pk == 4085603ULL);

    /* position 3 */
    const char *pos3 = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1";
    Position p3b;
    check("pos3 fen parse", pos_from_fen(&p3b, pos3) == 0);
    unsigned long long p3r = perft(&p3b, 5);
    check("pos3 perft5=674624", p3r == 674624ULL);

    /* weights load invalid */
    WeightSet ws;
    weights_defaults(&ws);
    check("defaults loaded", ws.loaded == 1);
    /* missing file returns error */
    int rr = weights_load(&ws, "/tmp/no_such_weight.bin");
    check("missing weights error", rr != 0);
    /* eval doesn't crash on missing file */
    Position tp; pos_init_startpos(&tp);
    WeightSet bad;
    memset(&bad,0,sizeof(bad));
    int sc = eval_position(&tp, &bad);
    check("eval works with unloaded", 1);

    /* fen roundtrip */
    Position orig; pos_init_startpos(&orig);
    char fbuf[256];
    pos_to_fen(&orig, fbuf, sizeof(fbuf));
    Position rep;
    check("fen roundtrip parse", pos_from_fen(&rep, fbuf) == 0);

    /* game result stalemate/mate maybe not */

    /* search smoke */
    WeightSet w; weights_defaults(&w);
    Position sp; pos_init_startpos(&sp);
    int move = search_best_move(&sp, &w, 4, 2000, &sc);
    check("search returns move", move >= 0);

    int passes = total - failures;
    /* overwrite line not possible; just ensure last line format */
    /* reprint? We already printed wrong? remove previous by not printing until now */
    /* We'll not print above; instead print now only */
    printf("%d/%d testes passaram\n", passes, total);
    return (failures == 0) ? 0 : 1;
}
