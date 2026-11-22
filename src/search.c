#include "search.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define INF 10000000
#define MATE 100000

long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec*1000L + ts.tv_nsec/1000000L;
}

static int piece_value(int code) {
    switch (code) {
        case WPAWN: case BPAWN: return 100;
        case WKNIGHT: case BKNIGHT: return 320;
        case WBISHOP: case BBISHOP: return 330;
        case WROOK: case BROOK: return 500;
        case WQUEEN: case BQUEEN: return 900;
        default: return 0;
    }
}

static void check_time(SearchContext *ctx) {
    if ((ctx->nodes & 2047) == 0) {
        long el = now_ms() - ctx->start_ms;
        if (el >= ctx->movetime_ms) ctx->stopped = 1;
    }
}

static int move_score_order(Position *pos, Move m, SearchContext *ctx, Move ttbest, int ply) {
    int from=MOVE_FROM(m), to=MOVE_TO(m);
    int piece=pos->board[from];
    int captured = pos->board[to];
    if (code_type_idx(piece)==0 && to==pos->ep && captured==EMPTY) captured = (code_color(piece)==WT)? BPAWN : WPAWN;
    int promo = MOVE_PROM(m);
    if (m == ttbest) return 10000000;
    int score = 0;
    if (captured != EMPTY) {
        score = 100000 + piece_value(captured)*10 - piece_value(piece);
    }
    if (promo) score += 90000;
    if (m == ctx->killer[ply][0]) score += 9000;
    if (m == ctx->killer[ply][1]) score += 8000;
    score += ctx->history[from][to];
    return score;
}

static void sort_moves(Move *moves, int n, int *scores) {
    for (int i=1;i<n;i++) {
        Move km = moves[i];
        int ks = scores[i];
        int j=i-1;
        while (j>=0 && scores[j] < ks) {
            moves[j+1]=moves[j];
            scores[j+1]=scores[j];
            j--;
        }
        moves[j+1]=km;
        scores[j+1]=ks;
    }
}

int quiescence(SearchContext *ctx, Position *pos, int alpha, int beta, int ply) {
    ctx->nodes++;
    check_time(ctx);
    if (ctx->stopped) return 0;

    int standpat = eval_score(pos, ctx->weights);
    if (standpat >= beta) return beta;
    if (standpat > alpha) alpha = standpat;

    Move pseudo[MAX_MOVES];
    int np = generate_pseudo_moves(pos, pseudo);
    Move caps[MAX_MOVES];
    int nc = 0;
    for (int i=0;i<np;i++) {
        int from=MOVE_FROM(pseudo[i]), to=MOVE_TO(pseudo[i]);
        int piece=pos->board[from];
        int captured=pos->board[to];
        int iscap = 0;
        if (captured != EMPTY && code_color(captured) != code_color(piece)) iscap=1;
        if (code_type_idx(piece)==0 && to==pos->ep && captured==EMPTY) iscap=1;
        if (MOVE_PROM(pseudo[i]) != 0) iscap=1;
        if (iscap) caps[nc++] = pseudo[i];
    }

    int scores[MAX_MOVES];
    Move legal[MAX_MOVES];
    int nl = 0;
    for (int i=0;i<nc;i++) {
        Undo u;
        make_move(pos, caps[i], &u);
        int prev=u.side;
        int lc = !in_check(pos, prev);
        unmake_move(pos,&u);
        if (!lc) continue;
        legal[nl] = caps[i];
        scores[nl] = move_score_order(pos, caps[i], ctx, 0, ply);
        nl++;
    }
    sort_moves(legal, nl, scores);

    for (int i=0;i<nl;i++) {
        Undo u;
        make_move(pos, legal[i], &u);
        int score = -quiescence(ctx, pos, -beta, -alpha, ply+1);
        unmake_move(pos, &u);
        if (ctx->stopped) return 0;
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

static int alphabeta(SearchContext *ctx, Position *pos, int depth, int alpha, int beta, int ply) {
    ctx->nodes++;
    check_time(ctx);
    if (ctx->stopped) return 0;
    if (ply > MAX_PLY-4) return eval_score(pos, ctx->weights);

    if (depth <= 0) return quiescence(ctx, pos, alpha, beta, ply);

    uint64_t key = pos->hash;
    TTEntry *e = &ctx->tt[key & ctx->tt_mask];
    int tt_hit = (e->key == key);
    if (tt_hit && e->depth >= depth) {
        if (e->flag == 0) return e->score;
        if (e->flag == 1 && e->score > alpha) alpha = e->score;
        if (e->flag == 2 && e->score < beta) beta = e->score;
        if (alpha >= beta) return e->score;
    }

    Move ttbest = tt_hit ? e->best : 0;

    Move moves[MAX_MOVES];
    int n = generate_legal_moves(pos, moves);
    if (n == 0) {
        if (in_check(pos, pos->side)) return -MATE + ply;
        return 0;
    }

    int scores[MAX_MOVES];
    for (int i=0;i<n;i++) scores[i] = move_score_order(pos, moves[i], ctx, ttbest, ply);
    sort_moves(moves, n, scores);

    int alpha0 = alpha;
    int best_score = -INF;
    Move best_move = 0;
    for (int i=0;i<n;i++) {
        Undo u;
        make_move(pos, moves[i], &u);
        int score = -alphabeta(ctx, pos, depth-1, -beta, -alpha, ply+1);
        unmake_move(pos, &u);
        if (ctx->stopped) return 0;

        if (score > best_score) { best_score = score; best_move = moves[i]; }
        if (score > alpha) alpha = score;
        if (alpha >= beta) {
            int from=MOVE_FROM(moves[i]), to=MOVE_TO(moves[i]);
            int piece=pos->board[from];
            int captured=pos->board[to];
            int iscap = (captured != EMPTY && code_color(captured)!=code_color(piece));
            if (code_type_idx(piece)==0 && to==pos->ep && captured==EMPTY) iscap=1;
            if (!iscap) {
                ctx->killer[ply][1] = ctx->killer[ply][0];
                ctx->killer[ply][0] = moves[i];
                int bonus = (depth*depth);
                if (bonus > 0) ctx->history[from][to] += bonus;
            }
            break;
        }
    }

    if (!ctx->stopped) {
        e->key = key;
        e->best = best_move;
        e->score = best_score;
        e->depth = depth;
        if (best_score <= alpha0) e->flag = 2;
        else if (best_score >= beta) e->flag = 1;
        else e->flag = 0;
    }
    return best_score;
}

Move search_best_move(SearchContext *ctx, Position *pos, int max_depth, int movetime_ms) {
    ctx->max_depth = max_depth;
    ctx->movetime_ms = movetime_ms;
    ctx->start_ms = now_ms();
    ctx->stopped = 0;
    ctx->nodes = 0;
    ctx->best_move = 0;
    ctx->best_score = 0;

    Move moves[MAX_MOVES];
    int n = generate_legal_moves(pos, moves);
    if (n == 0) return 0;
    if (n == 1) return moves[0];

    int depth;
    int best_prev = moves[0];
    for (depth=1; depth <= max_depth; depth++) {
        int alpha = -INF, beta = INF;
        Move best = 0;
        int best_score = -INF;
        int scores[MAX_MOVES];
        Move ttbest=0;
        if (ctx->tt) {
            TTEntry *te = &ctx->tt[pos->hash & ctx->tt_mask];
            if (te->key == pos->hash) ttbest = te->best;
        }
        for (int i=0;i<n;i++) scores[i] = move_score_order(pos, moves[i], ctx, ttbest, 0);
        sort_moves(moves, n, scores);

        ctx->stopped = 0;
        for (int i=0;i<n;i++) {
            Undo u;
            make_move(pos, moves[i], &u);
            int sc = -alphabeta(ctx, pos, depth-1, -beta, -alpha, 1);
            unmake_move(pos, &u);
            if (ctx->stopped) break;
            if (sc > best_score) { best_score = sc; best = moves[i]; }
            if (sc > alpha) alpha = sc;
        }
        if (!ctx->stopped) {
            if (best == 0) best = moves[0];
            ctx->reached_depth = depth;
            best_prev = best;
            ctx->best_move = best;
            ctx->best_score = best_score;
            if (best_score > MATE - 100 || best_score < -MATE + 100) break;
        } else {
            break;
        }
    }
    if (ctx->best_move == 0) ctx->best_move = best_prev;
    return ctx->best_move;
}

SearchContext *search_create(const float *weights, int tt_entries) {
    SearchContext *ctx = calloc(1, sizeof(SearchContext));
    if (!ctx) return NULL;
    if (tt_entries < 1024) tt_entries = 1024;
    int sz = 1;
    while (sz < tt_entries) sz <<= 1;
    ctx->tt = calloc(sz, sizeof(TTEntry));
    if (!ctx->tt) { free(ctx); return NULL; }
    ctx->tt_mask = sz - 1;
    ctx->weights = weights;
    return ctx;
}

void search_destroy(SearchContext *ctx) {
    if (!ctx) return;
    free(ctx->tt);
    free(ctx);
}
