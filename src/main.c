#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "position.h"
#include "eval.h"
#include "search.h"

static void sq_to_alg(int sq, char *buf) {
    buf[0] = 'a' + (sq % 8);
    buf[1] = '1' + (sq / 8);
    buf[2] = 0;
}

static void move_to_uci(Move m, char *buf) {
    char a[4], b[4];
    sq_to_alg(MOVE_FROM(m), a);
    sq_to_alg(MOVE_TO(m), b);
    int prom = MOVE_PROM(m);
    if (prom == 0) {
        snprintf(buf, 16, "%s%s", a, b);
    } else {
        char p;
        if (prom == 1) p = 'q';
        else if (prom == 2) p = 'r';
        else if (prom == 3) p = 'b';
        else p = 'n';
        snprintf(buf, 16, "%s%s%c", a, b, p);
    }
}

int main(int argc, char **argv) {
    char fen[1024] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    char apply_move[32] = "";
    char weights_path[512] = "";
    int movetime = 1000;
    int maxdepth = 64;

    for (int i=1;i<argc;i++) {
        if (strcmp(argv[i], "--fen") == 0 && i+1 < argc) {
            strncpy(fen, argv[++i], sizeof(fen)-1);
        } else if (strcmp(argv[i], "--weights") == 0 && i+1 < argc) {
            strncpy(weights_path, argv[++i], sizeof(weights_path)-1);
        } else if (strcmp(argv[i], "--movetime") == 0 && i+1 < argc) {
            movetime = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--depth") == 0 && i+1 < argc) {
            maxdepth = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--time") == 0 && i+1 < argc) {
            movetime = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--apply-move") == 0 && i+1 < argc) {
            strncpy(apply_move, argv[++i], sizeof(apply_move)-1);
        }
    }

    Position pos;
    char err[256];
    if (position_from_fen(&pos, fen, err, sizeof(err)) != 0) {
        fprintf(stderr, "invalid fen: %s\n", err);
        return 2;
    }

    if (apply_move[0]) {
        Move mm = /* parse UCI */ 0;
        /* parse move by matching generated moves to UCI string */
        Move cands[MAX_MOVES];
        int cn = generate_legal_moves(&pos, cands);
        char buf[16];
        int found = 0;
        for (int i=0;i<cn;i++) {
            move_to_uci(cands[i], buf);
            if (strcmp(buf, apply_move) == 0) { mm = cands[i]; found=1; break; }
        }
        if (!found) {
            fprintf(stderr, "illegal move %s\n", apply_move);
            return 6;
        }
        Undo u;
        make_move(&pos, mm, &u);
        int terminal = is_terminal(&pos, NULL);
        int mate = 0;
        if (terminal) {
            /* determine if new side has no moves and in check */
            Move nmv[MAX_MOVES];
            int nmm = generate_legal_moves(&pos, nmv);
            if (nmm == 0 && in_check(&pos, pos.side)) mate = 1;
        }
        if (mate) {
            printf("MATE\n");
        } else if (terminal) {
            printf("DRAW\n");
        } else {
            char fbuf[512];
            position_to_fen(&pos, fbuf, sizeof(fbuf));
            printf("CONTINUE %s\n", fbuf);
        }
        return 0;
    }

    Move legal[MAX_MOVES];
    int nm = generate_legal_moves(&pos, legal);
    if (nm == 0) {
        printf("0000\n");
        return 0;
    }
    if (nm == 1) {
        char buf[16];
        move_to_uci(legal[0], buf);
        printf("%s\n", buf);
        return 0;
    }

    float *weights = NULL;
    if (weights_path[0] == 0) {
        fprintf(stderr, "missing --weights\n");
        return 3;
    }
    if (eval_load_weights(weights_path, &weights, err, sizeof(err)) != 0) {
        fprintf(stderr, "weights error: %s\n", err);
        return 4;
    }

    SearchContext *ctx = search_create(weights, 1<<16);
    if (!ctx) {
        fprintf(stderr, "out of memory\n");
        free(weights);
        return 5;
    }
    if (movetime <= 0) movetime = 1000;
    if (movetime > 9000) movetime = 9000;

    Move m = search_best_move(ctx, &pos, maxdepth, movetime);
    if (m == 0) m = legal[0];

    char buf[16];
    fprintf(stderr, "max_depth=%d\n", ctx->reached_depth);
    move_to_uci(m, buf);
    printf("%s\n", buf);

    search_destroy(ctx);
    free(weights);
    return 0;
}