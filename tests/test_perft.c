#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/board.h"
#include "../src/types.h"

static int failed = 0;
static long perft(Board *b, int depth) {
    if (depth == 0) return 1;
    Move moves[4096]; int mc = 0;
    gen_moves(b, moves, &mc);
    long total = 0;
    for (int i = 0; i < mc; i++) {
        Board tmp = *b;
        Undo u; memset(&u,0,sizeof(u));
        make_move(&tmp, moves[i], &u);
        int orig = b->side;
        int ksq = __builtin_ctzll(tmp.pieces[orig][KING]);
        if (attacks_to(&tmp, ksq, tmp.side)) continue;
        total += perft(&tmp, depth-1);
    }
    return total;
}

static void check(const char *fen, int d, long expected) {
    Board b; board_load_fen(&b, fen);
    long got = perft(&b,d);
    printf("perft %s d=%d got=%ld exp=%ld\n", fen, d, got, expected);
    if (got != expected) { failed = 1; }
}

int main(void) {
    check("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609);
    check("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603);
    check("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624);
    return failed ? 1 : 0;
}
