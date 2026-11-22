#include "position.h"
#include "search.h"
#include "eval.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed=0, total=0;
static void check(const char *name, int cond){
    total++;
    if(cond){ passed++; }
    else { printf("FAIL: %s\n", name); }
}

int main(void){
    /* perft exact */
    {
        Position p=position_start();
        check("perft start d5", perft_position(&p,5)==4865609ULL);
    }
    {
        Position p; position_from_fen(&p,"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
        check("perft kiwipete d4", perft_position(&p,4)==4085603ULL);
    }
    {
        Position p; position_from_fen(&p,"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
        check("perft pos3 d5", perft_position(&p,5)==674624ULL);
    }

    /* FEN invalid */
    {
        Position p; check("invalid fen rejected", position_from_fen(&p,"not a fen")==-1);
    }
    {
        Position p; check("missing king rejected", position_from_fen(&p,"8/8/8/8/8/8/8/8 w - - 0 1")==-1);
    }
    {
        Position p; check("two white kings rejected", position_from_fen(&p,"K6K/8/8/8/8/8/8/8 w - - 0 1")==-1);
    }

    /* attacks no wrap */
    {
        Position p; position_from_fen(&p,"R7/8/8/8/8/8/8/8 w - - 0 1");
        check("rook a1 does not wrap to h1", square_attacked(&p, 7, COLOR_WHITE)==0);
    }
    {
        Position p; position_from_fen(&p,"R7/8/8/8/8/8/8/8 w - - 0 1");
        check("rook a1 attacks a2", square_attacked(&p, 8, COLOR_WHITE)==1);
    }
    /* blocking */
    {
        Position p; position_from_fen(&p,"R6K/P7/8/8/8/8/8/7k w - - 0 1");
        check("rook blocked by pawn on a3", square_attacked(&p, 16, COLOR_WHITE)==0);
    }

    /* mate/stalemate/termination */
    {
        Position p; position_from_fen(&p,"4k3/8/8/8/8/8/8/4K3 w - - 0 1");
        check("insufficient material draw", has_insufficient_material(&p)==1);
    }
    {
        Position p=position_start();
        p.halfmove=100;
        check("50-move terminal", is_terminal(&p)==1);
    }

    /* eval sanity */
    {
        Position c; position_from_fen(&c,"8/8/8/3N4/8/8/8/8 w - - 0 1");
        Position e; position_from_fen(&e,"8/8/8/8/8/8/3N4/8 w - - 0 1");
        EvalWeights ew; eval_weights_default(&ew);
        check("knight center better than edge-ish", evaluate_position(&c,&ew) > evaluate_position(&e,&ew));
    }
    {
        EvalWeights ew; eval_weights_default(&ew);
        check("load missing weights fails", eval_load_weights(&ew,"/tmp/definitely_missing_weights.bin")==-1);
    }

    /* load valid model */
    {
        EvalWeights ew;
        check("load model_0", eval_load_weights(&ew,"models/model_0.bin")==0);
    }

    /* nan/inf abort tested via Python/system later */

    /* movegen legal count start */
    {
        Position p=position_start();
        MoveList ml; movegen_legal(&p,&ml,0);
        check("start has 20 legal moves", ml.count==20);
    }

    /* no illegal moves property not fully here */

    printf("%d/%d testes passaram\n", passed, total);
    return (passed==total)?0:1;
}
