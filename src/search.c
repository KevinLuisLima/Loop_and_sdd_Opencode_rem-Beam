#include "search.h"
#include "eval.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static long now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000L + tv.tv_usec / 1000L;
}

void search_init(SearchContext* ctx, Position* p, const double* w, int timeMs) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->pos = p;
    ctx->weights = w;
    ctx->timeBudgetMs = timeMs > 0 ? timeMs : 1000;
    ctx->t0 = now_ms();
    ctx->bestMove.from = 0; ctx->bestMove.to = 0; ctx->bestMove.promo = 0; ctx->bestMove.captured = 255; ctx->bestMove.flags = 0;
    ctx->bestDepth = 0;
    /* TT size ~ 65536 entries */
    ctx->tt.size = 1 << 16;
    ctx->tt.entries = calloc(ctx->tt.size, sizeof(TTEntry));
}

void search_free(SearchContext* ctx) {
    if (ctx->tt.entries) free(ctx->tt.entries);
    ctx->tt.entries = NULL;
}

static void check_time(SearchContext* ctx) {
    if (ctx->stopped) return;
    if ((ctx->nodes & 2047) == 0) {
        if (now_ms() - ctx->t0 >= ctx->timeBudgetMs) ctx->stopped = 1;
    }
}


/* forward decl */
static int repetition_draw(Position* p);
static int quiesce(SearchContext* ctx, Position* p, int alpha, int beta);
static int alphabeta(SearchContext* ctx, Position* p, int depth, int alpha, int beta, int pv);

static int score_move(SearchContext* ctx, Position* p, Move m, Move ttMove) {
    (void)ctx;
    int score = 0;
    if (ttMove.from == m.from && ttMove.to == m.to && ttMove.promo == m.promo) score += 100000;
    int victim = (m.captured < 12) ? m.captured : -1;
    int attacker = p->mailbox[m.from];
    if (victim >= 0) {
        /* MVV-LVA */
        static const int val[6] = { 100, 300, 300, 500, 900, 0 };
        score += 10000 + val[victim % 6] * 10 - val[attacker % 6];
    }
    if (m.promo) score += 9000;
    if (m.flags & 1) score += 1000;
    return score;
}

static int alphabeta(SearchContext* ctx, Position* p, int depth, int alpha, int beta, int pv) {
    ctx->nodes++;
    check_time(ctx);
    if (ctx->stopped) return 0;

    if (p->halfmove >= 100) return 0;
    if (insufficient_material(p)) return 0;
    if (repetition_draw(p)) return 0;

    TTEntry* te = NULL;
    int ti = (int)(p->hash % ctx->tt.size);
    te = &ctx->tt.entries[ti];
    if (te->valid && te->key == p->hash && te->depth >= depth && !pv) {
        if (te->flag == 0) return te->score;
        if (te->flag == 1 && te->score >= beta) return te->score;
        if (te->flag == 2 && te->score <= alpha) return te->score;
    }

    if (depth <= 0) {
        return quiesce(ctx, p, alpha, beta);
    }

    MoveList ml;
    gen_moves(p, &ml, false);
    /* filter legal */
    Move moves[MAX_MOVES]; int nm = 0;
    for (int i = 0; i < ml.count; i++) {
        if (move_is_legal(p, ml.moves[i])) moves[nm++] = ml.moves[i];
    }
    if (nm == 0) {
        if (in_check(p, p->side)) return -MATE + (100 - depth);
        return 0;
    }

    /* sort */
    int scores[MAX_MOVES];
    Move ttMove = te->valid && te->key == p->hash ? te->move : (Move){255,255,0,255,0};
    for (int i = 0; i < nm; i++) scores[i] = score_move(ctx, p, moves[i], ttMove);
    /* insertion sort desc */
    for (int i = 1; i < nm; i++) {
        Move km = moves[i]; int ks = scores[i]; int j = i - 1;
        while (j >= 0 && scores[j] < ks) { moves[j+1] = moves[j]; scores[j+1] = scores[j]; j--; }
        moves[j+1] = km; scores[j+1] = ks;
    }

    Move bestMove = moves[0];
    int origAlpha = alpha;
    int best = -INF;
    for (int i = 0; i < nm; i++) {
        if (ctx->stopped) break;
        Undo u;
        make_move(p, moves[i], &u);
        int sc = -alphabeta(ctx, p, depth - 1, -beta, -alpha, 0);
        undo_move(p, &u);
        if (ctx->stopped) break;
        if (sc > best) { best = sc; bestMove = moves[i]; if (sc > alpha) alpha = sc; }
        if (alpha >= beta) break;
    }

    /* store TT */
    if (!ctx->stopped) {
        te->key = p->hash;
        te->depth = depth;
        te->score = best;
        te->move = bestMove;
        te->valid = 1;
        if (best <= origAlpha) te->flag = 2; /* upper */
        else if (best >= beta) te->flag = 1; /* lower */
        else te->flag = 0;
    }
    return best;
}

static int repetition_draw(Position* p) {
    int c = 0;
    for (int i = 0; i < p->historyCount; i++) if (p->hashHistory[i] == p->hash) c++;
    return c >= 2;
}

static int quiesce(SearchContext* ctx, Position* p, int alpha, int beta) {
    ctx->nodes++;
    check_time(ctx);
    if (ctx->stopped) return 0;
    if (p->halfmove >= 100 || insufficient_material(p) || repetition_draw(p)) return 0;
    int stand = evaluate(p, ctx->weights);
    if (stand >= beta) return beta;
    if (stand > alpha) alpha = stand;

    MoveList ml;
    gen_moves(p, &ml, true);
    Move moves[MAX_MOVES]; int nm = 0;
    for (int i = 0; i < ml.count; i++) {
        if (move_is_legal(p, ml.moves[i])) moves[nm++] = ml.moves[i];
    }
    int scores[MAX_MOVES];
    Move dummy = {255,255,0,255,0};
    for (int i = 0; i < nm; i++) scores[i] = score_move(ctx, p, moves[i], dummy);
    for (int i = 1; i < nm; i++) {
        Move km = moves[i]; int ks = scores[i]; int j = i - 1;
        while (j >= 0 && scores[j] < ks) { moves[j+1] = moves[j]; scores[j+1] = scores[j]; j--; }
        moves[j+1] = km; scores[j+1] = ks;
    }
    for (int i = 0; i < nm; i++) {
        if (ctx->stopped) break;
        Undo u;
        make_move(p, moves[i], &u);
        int sc = -quiesce(ctx, p, -beta, -alpha);
        undo_move(p, &u);
        if (ctx->stopped) break;
        if (sc > alpha) alpha = sc;
        if (alpha >= beta) break;
    }
    return alpha;
}
Move search_bestmove(SearchContext* ctx) {
    Position* root = ctx->pos;
    MoveList ml;
    gen_moves(root, &ml, false);
    Move moves[MAX_MOVES]; int nm = 0;
    for (int i = 0; i < ml.count; i++) if (move_is_legal(root, ml.moves[i])) moves[nm++] = ml.moves[i];
    if (nm == 0) { ctx->bestMove = moves[0]; return ctx->bestMove; }
    if (nm == 1) { ctx->bestMove = moves[0]; return ctx->bestMove; }

    ctx->bestMove = moves[0];
    for (int depth = 1; depth <= 64; depth++) {
        if (ctx->stopped) break;
        ctx->stopped = 0;
        int alpha = -INF, beta = INF;
        Move bestThis = ctx->bestMove;
        int bestScore = -INF;
        int scores[MAX_MOVES];
        Move ttMove = {255,255,0,255,0};
        TTEntry* te = &ctx->tt.entries[root->hash % ctx->tt.size];
        if (te->valid && te->key == root->hash) ttMove = te->move;
        for (int i = 0; i < nm; i++) scores[i] = score_move(ctx, root, moves[i], ttMove);
        for (int i = 0; i < nm; i++) if (moves[i].from==ctx->bestMove.from && moves[i].to==ctx->bestMove.to && moves[i].promo==ctx->bestMove.promo) scores[i] += 50000;
        for (int i = 1; i < nm; i++) {
            Move km = moves[i]; int ks = scores[i]; int j = i - 1;
            while (j >= 0 && scores[j] < ks) { moves[j+1] = moves[j]; scores[j+1] = scores[j]; j--; }
            moves[j+1] = km; scores[j+1] = ks;
        }
        for (int i = 0; i < nm; i++) {
            if (ctx->stopped) break;
            Undo u;
            make_move(root, moves[i], &u);
            int sc = -alphabeta(ctx, root, depth - 1, -beta, -alpha, 1);
            undo_move(root, &u);
            if (ctx->stopped) break;
            if (sc > bestScore) { bestScore = sc; bestThis = moves[i]; alpha = sc; }
            if (alpha >= beta) break;
        }
        if (!ctx->stopped) {
            ctx->bestMove = bestThis;
            ctx->bestDepth = depth;
            if (bestScore >= MATE - 10) break;
        }
    }
    return ctx->bestMove;
}

uint64_t search_nodes(SearchContext* ctx) { return ctx->nodes; }
