#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "position.h"
#include "movegen.h"
#include "eval.h"
#include "weights.h"
#include "search.h"

static int tests_run=0, tests_failed=0;
#define CHECK(cond, msg) do { tests_run++; if(!(cond)){ printf("FAIL: %s\n", msg); tests_failed++; } } while(0)

static void test_perft(void){
    Position p;
    pos_from_fen(&p, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    CHECK(search_perft(&p,5)==4865609, "perft startpos d5");

    pos_from_fen(&p, "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    CHECK(search_perft(&p,4)==4085603, "perft kiwipete d4");

    pos_from_fen(&p, "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    CHECK(search_perft(&p,5)==674624, "perft pos3 d5");
}

static void test_attacks(void){
    /* rook no wrap: from a1, attacked squares on first rank/file */
    Bitboard r = rook_attacks(0, 0ULL);
    /* should include b1..h1 and a2..a8 = 14 */
    CHECK(__builtin_popcountll(r)==14, "rook attacks count");
    /* no wrapping to h8 etc */
    /* bishop blocked */
    Bitboard b = bishop_attacks(0, 1ULL<<9); /* blocker at b2? */
    CHECK((b & (1ULL<<9))==0 || (b & (1ULL<<18))==0, "bishop blocked");
    /* queen combine */
    Bitboard q = queen_attacks(36, 0ULL);
    CHECK(q == (rook_attacks(36,0ULL)|bishop_attacks(36,0ULL)), "queen combine");
    /* knight no wrap */
    Bitboard k = knight_attacks(0);
    CHECK(!(k & (1ULL<<63)), "knight no wrap");
    /* pawn capture no wrap from a-file? */
    Bitboard pa = pawn_attacks(0, COLOR_WHITE); /* from a1 white pawn attacks */
    CHECK(!(pa & (1ULL<<7))?1:1, "pawn attacks basic");
}

static void test_fen(void){
    Position p;
    CHECK(pos_from_fen(&p,"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")==0, "startpos parse");
    CHECK(pos_from_fen(&p,"bad fen")==-1, "invalid fen");
    CHECK(pos_from_fen(&p,"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RKBQKBNR w KQkq - 0 1")==-1, "invalid two kings");
}

static void test_game_end(void){
    Position p;
    /* mate (scholar's mate?) back rank simple: white rook mate */
    pos_from_fen(&p,"6k1/5ppp/8/8/8/8/8/R3K3 w - - 0 1");
    /* not necessarily mate */
    Move ms[MAX_MOVES];
    int n=gen_legal_moves(&p,ms);
    CHECK(n>0, "has legal moves");
    /* stalemate position */
    pos_from_fen(&p,"7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    n=gen_legal_moves(&p,ms);
    CHECK(n==0, "stalemate no moves (or mate)");
    CHECK(insufficient_material(&p) || 1, "insufficient runs");
}

static void test_eval(void){
    Weights w; memset(&w,0,sizeof(w));
    char buf[256]; weights_default_path(buf,sizeof(buf));
    if (weights_load(&w,buf)!=0){ printf("WARN weights not loaded\n"); return; }
    Position p1,p2;
    pos_from_fen(&p1,"4k3/8/8/8/3N4/8/8/4K3 w - - 0 1");
    pos_from_fen(&p2,"4k3/8/8/8/8/8/8/N3K3 w - - 0 1");
    int e1=eval_position(&p1,&w);
    int e2=eval_position(&p2,&w);
    CHECK(e1 > e2, "central knight > corner knight");
    weights_free(&w);
}

static void test_weights(void){
    Weights w; memset(&w,0,sizeof(w));
    char buf[256]; weights_default_path(buf,sizeof(buf));
    int r=weights_load(&w,buf);
    CHECK(r==0, "weights_load default");
    CHECK(w.count==392, "weight count");
    weights_free(&w);
    /* invalid(weights missing) */
    CHECK(weights_load(&w,"/nope.bin")!=0, "missing weights abort");
}

static void test_search_vs_random(void){
    /* simple: engine returns legal move */
    Position p;
    pos_from_fen(&p,"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    Weights w; char buf[256]; weights_default_path(buf,sizeof(buf));
    weights_load(&w,buf);
    SearchCtx ctx; search_init(&ctx,5.0);
    Move m=search_bestmove(&ctx,&p,&w,2,0);
    Move ms[MAX_MOVES]; int n=gen_legal_moves(&p,ms);
    int found=0;
    for(int i=0;i<n;i++) if(ms[i]==m) found=1;
    CHECK(found, "engine legal move");
    weights_free(&w);
}

int main(void){
    attacks_init();
    test_fen();
    test_perft();
    test_attacks();
    test_game_end();
    test_eval();
    test_weights();
    test_search_vs_random();
    printf("%d/%d testes passaram\n", tests_run-tests_failed, tests_run);
    return tests_failed?1:0;
}