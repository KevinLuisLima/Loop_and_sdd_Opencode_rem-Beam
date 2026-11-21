#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "movegen.h"
#include "search.h"
#include <time.h>
#include <string.h>

typedef struct {
    struct timespec start;
    int budget_ms;
    volatile int stop;
    int nodes;
} Sctx;

static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static long long start_ms;

static int time_up(Sctx *ctx) {
    if (ctx->stop) return 1;
    long long elapsed = now_ms() - start_ms;
    if (elapsed >= ctx->budget_ms) { ctx->stop = 1; return 1; }
    return 0;
}

#define INF 1000000
#define MATE 90000

static int move_score(const Position *pos, int m) {
    /* simple: captures first MVV-LVA; promotions high */
    int to = (m >> 6) & 63;
    int promo = (m >> 12) & 7;
    if (promo) return 10000 + promo*100;
    int moverColor; /* current side? pos not moved */
    /* determine if capture */
    int target = pos_piece_at(pos, to, NULL);
    if (target != PT_NONE) {
        int from = m & 63;
        int attacker = pos_piece_at(pos, from, &moverColor);
        (void)attacker;
        return 1000 + target*100;
    }
    return 0;
}

static int alpha_beta(Position *pos, const WeightSet *ws, int depth, int alpha, int beta, int ply, Sctx *ctx, int *bestmove_out) {
    if (time_up(ctx)) return 0;
    ctx->nodes++;
    int moves[512];
    int n = movegen_legal_moves(pos, moves, 512);
    if (n == 0) {
        if (pos_in_check(pos, pos->side)) return -MATE + ply;
        return 0;
    }
    if (depth <= 0) {
        return eval_position(pos, ws);
    }
    /* order moves */
    /* simple selection sort by score descending */
    for (int i = 1; i < n; i++) {
        int key = moves[i];
        int ks = move_score(pos, key);
        int j = i - 1;
        while (j >= 0 && move_score(pos, moves[j]) < ks) {
            moves[j+1] = moves[j];
            j--;
        }
        moves[j+1] = key;
    }

    int best = -INF;
    int bestmv = moves[0];
    for (int i = 0; i < n; i++) {
        int m = moves[i];
        int from = m & 63;
        int to = (m >> 6) & 63;
        int promo = (m >> 12) & 7;
        Undo u;
        pos_make_move(pos, from, to, promo, &u);
        int score = -alpha_beta(pos, ws, depth-1, -beta, -alpha, ply+1, ctx, NULL);
        pos_unmake_move(pos, &u);
        if (ctx->stop) return best;
        if (score > best) { best = score; bestmv = m; }
        if (score > alpha) alpha = score;
        if (alpha >= beta) break;
    }
    if (bestmove_out) *bestmove_out = bestmv;
    return best;
}

int search_best_move(Position *pos, const WeightSet *ws, int max_depth, int time_ms, int *out_score) {
    int moves[512];
    int n = movegen_legal_moves(pos, moves, 512);
    if (n == 0) return -1;
    if (n == 1) { if (out_score) *out_score = 0; return moves[0]; }

    Sctx ctx; memset(&ctx, 0, sizeof(ctx));
    ctx.budget_ms = (time_ms > 0) ? time_ms : 5000;
    start_ms = now_ms();

    int bestmove = moves[0];
    int bestscore = 0;
    for (int depth = 1; depth <= max_depth; depth++) {
        if (time_up(&ctx)) break;
        long long depth_start = now_ms();
        int mv = bestmove;
        int sc = alpha_beta(pos, ws, depth, -INF, INF, 0, &ctx, &mv);
        if (ctx.stop) break;
        bestmove = mv;
        bestscore = sc;
        if (now_ms() - depth_start < 1) {} /* no-op */
        if (depth >= max_depth) break;
    }
    if (out_score) *out_score = bestscore;
    return bestmove;
}
