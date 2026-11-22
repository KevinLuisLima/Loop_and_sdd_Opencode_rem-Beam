#include "board.h"
#include "weights.h"
#include "search.h"
#include "types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    const char *wpath = NULL;
    int time_ms = 1000;
    int depth = 64;
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--fen") == 0 && i + 1 < argc) { fen = argv[++i]; }
        else if (strcmp(argv[i], "--weights") == 0 && i + 1 < argc) { wpath = argv[++i]; }
        else if (strcmp(argv[i], "--time") == 0 && i + 1 < argc) { time_ms = atoi(argv[++i]); }
        else if (strcmp(argv[i], "--depth") == 0 && i + 1 < argc) { depth = atoi(argv[++i]); }
    }
    Board b;
    if (board_load_fen(&b, fen) != 0) {
        fprintf(stderr, "invalid FEN\n");
        return 2;
    }
    if (b.pieces[WHITE][KING] == 0 || b.pieces[BLACK][KING] == 0) {
        fprintf(stderr, "invalid position: missing king\n");
        return 2;
    }
    Weights w;
    if (wpath) {
        char err[256];
        err[0] = 0;
        if (weights_load(wpath, &w, err, sizeof(err)) != 0) {
            fprintf(stderr, "weights error: %s\n", err);
            return 2;
        }
    } else {
        weights_default(&w);
        w.count = NUM_WEIGHTS;
    }
    SearchCtx ctx;
    search_init_ctx(&ctx, &w);
    Move best = 0;
    int depth_reached = 0;
    search_bestmove(&b, &ctx, depth, time_ms, &best, &depth_reached);
    search_free_ctx(&ctx);
    if (best == 0) {
        Move moves[4096];
        int mc = 0;
        gen_moves(&b, moves, &mc);
        if (mc > 0) best = moves[0];
    }
    if (best == 0) {
        fprintf(stderr, "no legal move\n");
        return 1;
    }
    char promo = 0;
    if (MOVE_FLAGS(best) == FLAG_PROMO) {
        int code = MOVE_PROMO(best);
        if (code == 1) promo = 'n';
        else if (code == 2) promo = 'b';
        else if (code == 3) promo = 'r';
        else promo = 'q';
    }
    if (promo)
        printf("bestmove %c%c%c%c%c\n", 'a' + MOVE_FROM(best)%8, '1' + MOVE_FROM(best)/8, 'a' + MOVE_TO(best)%8, '1' + MOVE_TO(best)/8, promo);
    else
        printf("bestmove %c%c%c%c\n", 'a' + MOVE_FROM(best)%8, '1' + MOVE_FROM(best)/8, 'a' + MOVE_TO(best)%8, '1' + MOVE_TO(best)/8);
    printf("score cp %d depth %d\n", ctx.last_score, depth_reached);
    return 0;
}
