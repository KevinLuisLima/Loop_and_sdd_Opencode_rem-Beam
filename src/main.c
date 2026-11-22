#include "board.h"
#include "eval.h"
#include "search.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s FEN [weights] [timeMs] | perft depth FEN\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "perft") == 0) {
        if (argc < 4) { fprintf(stderr, "perft depth FEN\n"); return 1; }
        int depth = atoi(argv[2]);
        char fen[512]; fen[0] = 0;
        for (int i = 3; i < argc; i++) { if (i > 3) strcat(fen, " "); strcat(fen, argv[i]); }
        Position p; pos_init(&p);
        if (pos_from_fen(&p, fen) != 0) { fprintf(stderr, "bad fen\n"); return 1; }
        uint64_t n = perft(&p, depth);
        printf("%llu\n", (unsigned long long)n);
        return 0;
    }

    char fen[512]; strncpy(fen, argv[1], 511); fen[511] = 0;
    /* if FEN has spaces, user should quote; but join if extra args look like fen tokens? handle: if argc>2 and argv[2] contains '/' treat as continuation */
    // allow FEN passed in multiple args until a file path or number
    // collect tokens that are not weights path or number
    // simple: if argc>=3 and argv[2][0] != '/' && !atoi(argv[2])? We'll just use argv[1].
    const char* wpath = "weights.bin";
    int timeMs = 7000;
    if (argc >= 3) {
        /* determine if argv[2] is weights file path */
        if (strchr(argv[2], '.') || strchr(argv[2], '/') || strcmp(argv[2]+strlen(argv[2])-4, ".bin")==0) {
            wpath = argv[2];
            if (argc >= 4) timeMs = atoi(argv[3]);
        } else {
            /* maybe time only */
            int t = atoi(argv[2]);
            if (t > 0) timeMs = t;
            else wpath = argv[2];
            if (argc >= 4) {
                int t2 = atoi(argv[3]);
                if (t2 > 0) timeMs = t2; else wpath = argv[3];
            }
        }
    }

    Position p; pos_init(&p);
    if (pos_from_fen(&p, fen) != 0) {
        fprintf(stderr, "invalid fen\n");
        return 2;
    }

    double w[N_WEIGHTS];
    if (load_weights(wpath, w, N_WEIGHTS) != 0) {
        fprintf(stderr, "weights load failed\n");
        return 3;
    }

    SearchContext ctx;
    search_init(&ctx, &p, w, timeMs);
    Move m = search_bestmove(&ctx);
    if (getenv("DEBUG_DEPTH")) fprintf(stderr, "MAX_DEPTH=%d\n", ctx.bestDepth);
    char buf[16];
    printf("%s\n", move_to_uci(m, buf));
    search_free(&ctx);
    return 0;
}
