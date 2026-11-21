#include "board.h"
#include "movegen.h"
#include "search.h"
#include "weights.h"
#include "eval.h"
#include "common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

static int total=0, passed=0;
#define EXPECT(cond) do { total++; if (cond) { passed++; } else { fprintf(stderr, "FAIL: %s (line %d)\n", #cond, __LINE__); } } while(0)

static long perft(Board *b, int depth) {
    if (depth == 0) return 1;
    MoveList ml; ml.count=0;
    generateLegalMoves(b,&ml);
    if (depth == 1) return ml.count;
    long nodes=0;
    for (int i=0;i<ml.count;i++) {
        makeMove(b, ml.moves[i]);
        nodes += perft(b, depth-1);
        unmakeMove(b);
    }
    return nodes;
}

int main(void) {
    /* initial legal moves */
    {
        Board b; boardSetStartpos(&b);
        MoveList ml; ml.count=0; generateLegalMoves(&b,&ml);
        EXPECT(ml.count == 20);
    }
    /* perft depth 5 initial */
    {
        Board b; boardSetStartpos(&b);
        EXPECT(perft(&b,5) == 4865609);
    }
    /* kiwipete depth 4 */
    {
        Board b; boardFromFEN(&b, "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
        EXPECT(perft(&b,4) == 4085603);
    }
    /* position 3 depth 5 */
    {
        Board b; boardFromFEN(&b, "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
        EXPECT(perft(&b,5) == 674624);
    }
    /* FEN roundtrip basic */
    {
        char buf[256];
        Board b; boardSetStartpos(&b);
        boardToFEN(&b,buf,sizeof(buf));
        Board b2; boardFromFEN(&b2,buf);
        EXPECT(b2.side==WHITE && b2.castling==b.castling);
    }
    /* weights validation */
    {
        Weights w; memset(&w,0,sizeof(w));
        w.w[0]=1.0f; EXPECT(weightsValidate(&w)==1);
        w.w[1]=INFINITY; EXPECT(weightsValidate(&w)==0);
    }
    /* make/unmake reversibility checked via perft counts (above) */
    /* engine returns legal move from start */
    {
        Board b; boardSetStartpos(&b);
        Weights w; memset(&w,0,sizeof(w));
        w.w[0]=100; w.w[1]=320; w.w[2]=330; w.w[3]=500; w.w[4]=900; w.count=EXPECTED_WEIGHTS;
        SearchStats st; Move m=searchBestMove(&b,&w,100,&st);
        MoveList ml; ml.count=0; generateLegalMoves(&b,&ml);
        int legal=0;
        for (int i=0;i<ml.count;i++) if (ml.moves[i].from==m.from && ml.moves[i].to==m.to && ml.moves[i].promo==m.promo) legal=1;
        EXPECT(legal==1);
    }
    printf("%d/%d testes passaram\n", passed, total);
    return (passed==total)?0:1;
}