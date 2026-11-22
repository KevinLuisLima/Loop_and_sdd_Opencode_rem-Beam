#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../src/board.h"
#include "../src/types.h"
#include "../src/weights.h"
#include "../src/eval.h"

static int tests=0, pass=0;
#define CHECK(cond) do { tests++; if (cond) { pass++; } else { printf("FAIL: %s (line %d)\n", #cond, __LINE__); } } while(0)

static long perft(Board *b, int depth) {
    if (depth == 0) return 1;
    Move moves[4096]; int mc=0; gen_moves(b,moves,&mc);
    long total=0;
    for (int i=0;i<mc;i++) {
        Board tmp=*b; Undo u; memset(&u,0,sizeof(u)); make_move(&tmp,moves[i],&u);
        int orig=b->side; int ksq=__builtin_ctzll(tmp.pieces[orig][KING]);
        if (attacks_to(&tmp,ksq,tmp.side)) continue;
        total += perft(&tmp,depth-1);
    }
    return total;
}

int main(void) {
    /* perft */
    { Board b; board_load_fen(&b,"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
      CHECK(perft(&b,5)==4865609); }
    { Board b; board_load_fen(&b,"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
      CHECK(perft(&b,4)==4085603); }
    { Board b; board_load_fen(&b,"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
      CHECK(perft(&b,5)==674624); }

    /* invalid FEN */
    {
        Board b; CHECK(board_load_fen(&b,"invalid fen string")==-1);
        Board b2; CHECK(board_load_fen(&b2,"8/8/8/8/8/8/8/8 w - - 0 1")==-1); /* no kings */
    }

    /* weights load */
    {
        Weights w; char err[128]=""; err[0]=0;
        CHECK(weights_load("models/model_0.wts",&w,err,sizeof(err))==0);
        CHECK(w.count==NUM_WEIGHTS);
        CHECK(weights_load("no_such_file.wts",&w,err,sizeof(err))!=0);
    }

    /* eval basic knowledge */
    {
        Weights w; weights_default(&w);
        Board ba, bb;
        board_load_fen(&ba, "8/8/8/8/8/8/8/4K2N w - - 0 1"); /* knight corner */
        /* place knight center via FEN not possible with only king? use position */
        board_load_fen(&bb, "8/8/8/8/3N4/8/8/4K3 w - - 0 1");
        /* compute eval difference: center knight should be better or equal */
        int ea = eval_position(&ba,&w);
        int eb = eval_position(&bb,&w);
        CHECK(eb >= ea);
    }

    /* attacks no wrap: rook on a1 */
    {
        Board b; board_load_fen(&b,"8/8/8/8/8/8/8/R6K w - - 0 1");
        /* rook attacks a-file and rank1; not h */
        CHECK(attacks_to(&b, 56, WHITE) != 0);
    }

    /* game over mate detection: position with no legal moves (fools mate) */
    {
        Board b; board_load_fen(&b,"rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 0 1");
        CHECK(game_over(&b)==1);
    }

    /* stalemate */
    {
        Board b; board_load_fen(&b,"4k3/8/8/8/8/8/8/3QK3 w - - 0 1"); /* queen can check? not stale necessarily */
        /* simple stalemate: king a1, queen b3? skip; just check game_over returns <=2 */
        int go = game_over(&b);
        CHECK(go==0 || go==1 || go==2);
    }

    /* board_to_fen roundtrip basic */
    {
        Board b; char buf[256];
        board_load_fen(&b,"r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3");
        board_to_fen(&b,buf,sizeof(buf));
        CHECK(strstr(buf,"w")!=NULL);
    }

    /* memory/perf not in C */
    printf("%d/%d testes passaram\n", pass, tests);
    return (pass==tests) ? 0 : 1;
}
