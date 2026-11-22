#include "search.h"
#include "movegen.h"
#include "eval.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

static double now_seconds(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC,&ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec/1e9;
}

static void sort_moves_basic(Position *p, Move *moves, int n, Move ttmove){
    (void)p;
    /* move ttmove to front if present */
    if (ttmove){
        for (int i=0;i<n;i++){
            if (moves[i]==ttmove){ Move t=moves[0]; moves[0]=moves[i]; moves[i]=t; break; }
        }
    }
    /* stable: captures before quiets (already maybe); simple insertion by capture flag */
    for (int i=1;i<n;i++){
        Move key=moves[i];
        int keyscore = (MOVE_FLAGS(key)==MF_CAPTURE||MOVE_FLAGS(key)==MF_PROMO||MOVE_FLAGS(key)==MF_EP)? 1 : 0;
        int j=i-1;
        while(j>=0){
            Move m=moves[j];
            int ms=(MOVE_FLAGS(m)==MF_CAPTURE||MOVE_FLAGS(m)==MF_PROMO||MOVE_FLAGS(m)==MF_EP)?1:0;
            if (ms>=keyscore) break;
            moves[j+1]=moves[j]; j--;
        }
        moves[j+1]=key;
    }
}


static int qsearch(SearchCtx *ctx, Position *p, Weights *w, int alpha, int beta, int ply){
    if (ctx->stop) return 0;
    if ((ctx->nodes & 1023)==0){
        if (now_seconds()-ctx->start_time >= ctx->time_limit) ctx->stop=1;
        if (ctx->stop) return 0;
    }
    ctx->nodes++;
    int standpat = eval_position(p,w);
    if (standpat >= beta) return standpat;
    if (standpat > alpha) alpha=standpat;

    Move moves[MAX_MOVES];
    int n=gen_pseudo_moves(p,moves,1);
    /* filter legal */
    Move legal[MAX_MOVES]; int ln=0;
    for (int i=0;i<n;i++){
        Undo u;
        make_move(p,moves[i],&u);
        int side_moved = p->side ^ 1;
        int inchk = in_check(p, side_moved);
        if (!inchk){
            /* if in check side? capture list still includes all, make sure not illegal */
            if (ln<MAX_MOVES) legal[ln++]=moves[i];
        }
        undo_move(p,&u);
    }
    /* order: none */
    for (int i=0;i<ln;i++){
        Undo u;
        make_move(p,legal[i],&u);
        int score = -qsearch(ctx,p,w,-beta,-alpha,ply+1);
        undo_move(p,&u);
        if (ctx->stop) return 0;
        if (score >= beta) return score;
        if (score > alpha) alpha=score;
    }
    return alpha;
}

static int alphabeta(SearchCtx *ctx, Position *p, Weights *w, int depth, int alpha, int beta, int ply){
    if (ctx->stop) return 0;
    if ((ctx->nodes & 1023)==0){
        if (now_seconds()-ctx->start_time >= ctx->time_limit) ctx->stop=1;
        if (ctx->stop) return 0;
    }
    /* repetition/draw checks */
    for (int i=0;i<ctx->history_count;i++){
        if (ctx->history[i]==p->hash) return 0;
    }
    if (p->halfmove>=100) return 0;
    if (insufficient_material(p)) return 0;

    TtEntry *te=tt_probe(&ctx->tt,p->hash);
    if (te && te->depth >= depth && te->key==p->hash){
        int sc=te->score;
        if (te->flags==0) return sc;
        if (te->flags==1 && sc>=beta) return sc; /* lower bound */
        if (te->flags==2 && sc<=alpha) return sc; /* upper bound */
    }
    if (depth<=0) return qsearch(ctx,p,w,alpha,beta,ply);

    Move moves[MAX_MOVES];
    int n=gen_pseudo_moves(p,moves,0);
    /* filter legal */
    Move legal[MAX_MOVES]; int ln=0;
    for(int i=0;i<n;i++){
        Undo u;
        make_move(p,moves[i],&u);
        int side_moved=p->side^1;
        if(!in_check(p,side_moved)){
            if(ln<MAX_MOVES) legal[ln++]=moves[i];
        }
        undo_move(p,&u);
    }
    if (ln==0){
        if (in_check(p,p->side)) return -MATE_SCORE + ply;
        return 0;
    }
    Move ttmove = (te)? te->move : 0;
    sort_moves_basic(p,legal,ln,ttmove);

    /* push hash */
    int pushed=0;
    if (ctx->history_count < SEARCH_HISTORY){ ctx->history[ctx->history_count++]=p->hash; pushed=1; }

    int origAlpha=alpha;
    int bestScore=-INF_SCORE;
    Move bestMove=legal[0];
    for (int i=0;i<ln;i++){
        Undo u;
        make_move(p,legal[i],&u);
        int score = -alphabeta(ctx,p,w,depth-1,-beta,-alpha,ply+1);
        undo_move(p,&u);
        if (ctx->stop) { if(pushed) ctx->history_count--; return 0; }
        if (score > bestScore){ bestScore=score; bestMove=legal[i]; }
        if (score > alpha) alpha=score;
        if (alpha >= beta) break;
    }
    if (pushed) ctx->history_count--;

    /* root tracking */
    if (ply==0){
        ctx->best_root_move=bestMove;
        ctx->best_root_score=bestScore;
    }

    /* TT store */
    int flag;
    if (bestScore <= origAlpha) flag=2;
    else if (bestScore >= beta) flag=1;
    else flag=0;
    tt_store(&ctx->tt,p->hash,depth,bestScore,flag,bestMove);
    return bestScore;
}

void search_init(SearchCtx *ctx, double time_limit_seconds){
    if(!ctx) return;
    tt_clear(&ctx->tt);
    ctx->nodes=0;
    ctx->stop=0;
    ctx->start_time=0;
    ctx->time_limit = time_limit_seconds>0? time_limit_seconds : 1.0;
    ctx->history_count=0;
    ctx->best_root_move=0;
    ctx->best_root_score=0;
    ctx->best_depth=0;
}

Move search_bestmove(SearchCtx *ctx, Position *root, Weights *w, int fixed_depth, double fixed_time_seconds){
    if(!ctx||!root) return 0;
    tt_clear(&ctx->tt);
    ctx->nodes=0;
    ctx->stop=0;
    ctx->history_count=0;
    ctx->best_root_move=0;
    double limit = 1.0;
    if (fixed_time_seconds>0) limit=fixed_time_seconds;
    else if (ctx->time_limit>0) limit=ctx->time_limit;
    if (limit <=0) limit=1.0;
    ctx->time_limit=limit;
    ctx->start_time=now_seconds();

    Move moves[MAX_MOVES];
    int n=gen_legal_moves(root,moves);
    if (n==0) return 0;
    if (n==1){ return moves[0]; }
    Move last_best=moves[0];

    int maxDepth = (fixed_depth>0)? fixed_depth : 64;
    for (int d=1; d<=maxDepth; d++){
        ctx->stop=0;
        ctx->best_root_move=0;
        (void)alphabeta(ctx,root,w,d,-INF_SCORE,INF_SCORE,0);
        if (ctx->stop){ break; }
        if (ctx->best_root_move){ last_best=ctx->best_root_move; ctx->best_depth=d; }
        /* time check for next depth */
        double elapsed = now_seconds()-ctx->start_time;
        if (fixed_depth>0 && d==fixed_depth) break;
        if (fixed_depth<=0){
            if (elapsed >= 0.7*limit) break;
        }
    }
    ctx->best_root_move = last_best;
    if (!ctx->best_root_move) ctx->best_root_move = moves[0];
    return ctx->best_root_move;
}

int search_perft(Position *p, int depth){
    if (depth==0) return 1;
    Move moves[MAX_MOVES];
    int n=gen_legal_moves(p,moves);
    if (depth==1) return n;
    int total=0;
    for (int i=0;i<n;i++){
        Undo u;
        make_move(p,moves[i],&u);
        total += search_perft(p,depth-1);
        undo_move(p,&u);
    }
    return total;
}