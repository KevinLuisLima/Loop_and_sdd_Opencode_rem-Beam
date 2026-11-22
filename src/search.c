#include "search.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct { uint64_t key; int score; int depth; int flag; Move move; } TTEntry;

static long now_ms(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000L + (long)(ts.tv_nsec / 1000000L);
}

void search_init_ctx(SearchCtx *ctx, const Weights *w) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->weights = w;
    /* TT ~ 1MB */
    ctx->tt_entries = 1 << 16;
    ctx->tt = calloc(ctx->tt_entries, sizeof(TTEntry));
}

void search_free_ctx(SearchCtx *ctx) {
    free(ctx->tt);
    ctx->tt = NULL;
}

static TTEntry *tt_lookup(SearchCtx *ctx, uint64_t key) {
    size_t idx = (size_t)(key % ctx->tt_entries);
    TTEntry *e = (TTEntry*)ctx->tt + idx;
    if (e->key == key) return e;
    return NULL;
}

static void tt_store(SearchCtx *ctx, uint64_t key, int depth, int score, int flag, Move move) {
    size_t idx = (size_t)(key % ctx->tt_entries);
    TTEntry *e = (TTEntry*)ctx->tt + idx;
    if (e->key != key || depth >= e->depth) {
        e->key = key; e->depth = depth; e->score = score; e->flag = flag; e->move = move;
    }
}

static const int PIECE_VAL[6] = {100,300,310,500,900,0};

static int repetition_draw(const Board *b);
static int move_score(const Board *b, Move m, SearchCtx *ctx, int ply, const TTEntry *tte) {
    if (tte && tte->move == m) return 1000000;
    int from = MOVE_FROM(m), to = MOVE_TO(m), flags = MOVE_FLAGS(m);
    /* captures MVV-LVA */
    if (flags == FLAG_CAPTURE || flags == FLAG_EP) {
        int victim = 0;
        if (flags == FLAG_EP) victim = 0; /* pawn */
        else {
            int opp = b->side ^ 1;
            for (int p = 0; p < 6; p++) if (b->pieces[opp][p] & (1ULL << to)) victim = p;
        }
        int attacker = 0;
        for (int p = 0; p < 6; p++) if (b->pieces[b->side][p] & (1ULL << from)) attacker = p;
        return 10000 + PIECE_VAL[victim] * 10 - PIECE_VAL[attacker];
    }
    if (flags == FLAG_PROMO) return 9000 + (MOVE_PROMO(m) == 4 ? 100 : 0);
    /* killer */
    if (ply < 64) {
        if (ctx->killers[ply][0] == m) return 8000;
        if (ctx->killers[ply][1] == m) return 7000;
    }
    /* history */
    return ctx->history[b->side][from][to];
}

static void order_moves(Move *moves, int n, const Board *b, SearchCtx *ctx, int ply, const TTEntry *tte) {
    /* simple insertion by score desc */
    static int scores[4096];
    for (int i = 0; i < n; i++) scores[i] = move_score(b, moves[i], ctx, ply, tte);
    for (int i = 1; i < n; i++) {
        Move km = moves[i]; int ks = scores[i]; int j = i - 1;
        while (j >= 0 && scores[j] < ks) { moves[j+1]=moves[j]; scores[j+1]=scores[j]; j--; }
        moves[j+1]=km; scores[j+1]=ks;
    }
}

static int quiesce(Board *b, SearchCtx *ctx, int alpha, int beta, int ply) {
    if (ctx->stop) return 0;
    ctx->nodes++;
    if (ctx->limit_ms > 0 && (ctx->nodes % 2048 == 0)) {
        if (now_ms() - ctx->start_ms >= ctx->limit_ms) ctx->stop = 1;
        if (ctx->stop) return 0;
    }
    int stand = eval_position(b, ctx->weights);
    if (stand >= beta) return stand;
    if (stand > alpha) alpha = stand;
    Move moves[4096]; int mc = 0;
    gen_captures(b, moves, &mc);
    TTEntry dummy; memset(&dummy,0,sizeof(dummy));
    order_moves(moves, mc, b, ctx, ply, NULL);
    for (int i = 0; i < mc; i++) {
        Board tmp = *b;
        Undo u; memset(&u,0,sizeof(u));
        make_move(&tmp, moves[i], &u);
        int orig = b->side;
        int ksq = __builtin_ctzll(tmp.pieces[orig][KING]);
        if (attacks_to(&tmp, ksq, tmp.side)) { /* illegal */
            /* unmake not needed */
            continue;
        }
        int score = -quiesce(&tmp, ctx, -beta, -alpha, ply + 1);
        if (ctx->stop) return 0;
        if (score > alpha) {
            alpha = score;
            if (alpha >= beta) {
                /* killer/history */
                break;
            }
        }
    }
    return alpha;
}

static int negamax(Board *b, SearchCtx *ctx, int depth, int alpha, int beta, int ply, Move *bestmove) {
    if (ctx->stop) return 0;
    ctx->nodes++;
    if (ctx->limit_ms > 0 && (ctx->nodes % 1024 == 0)) {
        if (now_ms() - ctx->start_ms >= ctx->limit_ms) { ctx->stop = 1; return 0; }
    }

    /* TT probe */
    uint64_t key = b->hash;
    TTEntry *tte = tt_lookup(ctx, key);
    if (tte && tte->depth >= depth && ply > 0) {
        if (tte->flag == 0) return tte->score;
        if (tte->flag == 1 && tte->score >= beta) return tte->score;
        if (tte->flag == 2 && tte->score <= alpha) return tte->score;
    }

    if (depth <= 0) {
        return quiesce(b, ctx, alpha, beta, ply);
    }

    /* game over checks */
    Move moves[4096]; int mc = 0;
    gen_moves(b, moves, &mc);
    if (mc == 0) {
        if (in_check(b, b->side)) return -100000 + ply; /* mate */
        return 0; /* stalemate */
    }

    /* 50-move / repetition draw */
    if (b->halfmove >= 100) return 0;
    if (ply > 0 && repetition_draw(b)) return 0;

    order_moves(moves, mc, b, ctx, ply, tte);

    int orig_alpha = alpha;
    Move best = 0;
    int best_score = -1000000;
    for (int i = 0; i < mc; i++) {
        Board tmp = *b;
        Undo u; memset(&u, 0, sizeof(u));
        make_move(&tmp, moves[i], &u);
        int orig = b->side;
        int ksq = __builtin_ctzll(tmp.pieces[orig][KING]);
        if (attacks_to(&tmp, ksq, tmp.side)) continue; /* illegal safety */
        Move child_best = 0;
        int score = -negamax(&tmp, ctx, depth - 1, -beta, -alpha, ply + 1, &child_best);
        if (ctx->stop) { 
            /* move unmake not needed (copy) */
            return 0; 
        }
        if (score > best_score) {
            best_score = score;
            best = moves[i];
        }
        if (score > alpha) alpha = score;
        if (alpha >= beta) {
            /* store killer/history for quiet moves */
            int iscap = (MOVE_FLAGS(moves[i]) == FLAG_CAPTURE || MOVE_FLAGS(moves[i]) == FLAG_EP);
            if (!iscap) {
                if (ply < 64) {
                    if (ctx->killers[ply][0] != moves[i]) {
                        ctx->killers[ply][1] = ctx->killers[ply][0];
                        ctx->killers[ply][0] = moves[i];
                    }
                }
                ctx->history[b->side][MOVE_FROM(moves[i])][MOVE_TO(moves[i])] += depth * depth;
            }
            break;
        }
    }

    int flag = 0;
    if (best_score <= orig_alpha) flag = 2; /* upper */
    else if (best_score >= beta) flag = 1; /* lower */
    else flag = 0;
    tt_store(ctx, key, depth, best_score, flag, best);

    if (bestmove && ply == 0) *bestmove = best;
    return best_score;
}

/* helper for repetition_draw using Board copy */
static int repetition_draw(const Board *b) {
    /* b side to move; if current hash occurred before */
    for (int i = 0; i < b->hash_count - 1; i++) {
        if (b->hash_history[i] == b->hash) return 1;
    }
    return 0;
}

int search_bestmove(Board *root, SearchCtx *ctx, int max_depth, int time_ms, Move *bestmove, int *depth_out) {
    ctx->stop = 0;
    ctx->nodes = 0;
    ctx->start_ms = now_ms();
    ctx->limit_ms = time_ms;
    ctx->max_depth = max_depth;
    ctx->depth_reached = 0;
    *bestmove = 0;
    Move best = 0;

    /* reset killers/history? keep */
    /* iterative deepening */
    for (int d = 1; d <= max_depth; d++) {
        if (ctx->stop) break;
        ctx->nodes = 0; /* per depth? keep cumulative? keep */
        Move cur_best = 0;
        int sc = negamax(root, ctx, d, -10000000, 10000000, 0, &cur_best);
        if (ctx->stop) {
            break;
        }
        if (cur_best) best = cur_best;
        ctx->depth_reached = d;
        ctx->last_score = sc;
        if (bestmove) *bestmove = best;
        if (depth_out) *depth_out = ctx->depth_reached;
        /* if mate found */
        if (sc > 999000 || sc < -999000) break;
        if (ctx->limit_ms > 0) {
            long elapsed = now_ms() - ctx->start_ms;
            if (elapsed >= ctx->limit_ms * 70 / 100) break;
        }
    }
    if (bestmove) *bestmove = best;
    if (depth_out) *depth_out = ctx->depth_reached;
    return 0;
}
