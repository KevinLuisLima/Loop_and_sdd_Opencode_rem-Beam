#include "search.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static long now_ms(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC,&ts);
    return ts.tv_sec*1000L + ts.tv_nsec/1000000L;
}

int search_eval_template(const Position *p, const EvalWeights *ew){
    return evaluate_position(p,ew);
}

int search_order_moves_template(const Position *p, MoveList *ml, int tt_move, const EvalWeights *ew){
    (void)ew;
    for(int i=0;i<ml->count;i++){
        Move *m=&ml->moves[i];
        int code = (m->from<<8)|(m->to<<2)|(m->promo & 3);
        if(tt_move!=0 && code==tt_move){ m->score=1000000; continue; }
        if(m->promo != PIECE_NONE){ m->score = 900000 + m->promo; continue; }
        int cap=-1;
        for(int c=0;c<2;c++) for(int t=0;t<6;t++){
            if(p->pieces[c][t] & (1ULL<<m->to)) cap=t;
        }
        if(m->to==p->ep_square) cap=PIECE_PAWN;
        if(cap!=-1){
            int attacker=-1;
            for(int c=0;c<2;c++) for(int t=0;t<6;t++){
                if(p->pieces[c][t] & (1ULL<<m->from)) attacker=t;
            }
            int vval = (cap==PIECE_PAWN?1:cap==PIECE_KNIGHT?3:cap==PIECE_BISHOP?3:cap==PIECE_ROOK?5:cap==PIECE_QUEEN?9:0);
            int aval = (attacker==PIECE_PAWN?1:attacker==PIECE_KNIGHT?3:attacker==PIECE_BISHOP?3:attacker==PIECE_ROOK?5:attacker==PIECE_QUEEN?9:1);
            m->score = 100000 + vval*10 - aval;
        } else {
            m->score = 0;
        }
    }
    /* insertion sort desc */
    for(int i=1;i<ml->count;i++){
        Move key=ml->moves[i];
        int j=i-1;
        while(j>=0 && ml->moves[j].score < key.score){
            ml->moves[j+1]=ml->moves[j];
            j--;
        }
        ml->moves[j+1]=key;
    }
    return 0;
}

void search_init(SearchContext *ctx, EvalWeights *ew){
    memset(ctx,0,sizeof(*ctx));
    ctx->weights=ew;
    ctx->evaluate = search_eval_template;
    ctx->tt.size = 1<<16;
    ctx->tt.entries = calloc(ctx->tt.size, sizeof(TTEntry));
}

void search_free(SearchContext *ctx){
    free(ctx->tt.entries);
    ctx->tt.entries=NULL;
}

static int packed_move(const Move *m){ return (m->from<<8)|(m->to<<2)|(m->promo & 3); }

static TTEntry* tt_probe(TranspositionTable *tt, uint64_t key){
    TTEntry *e = &tt->entries[key & (tt->size-1)];
    if(e->key==key) return e;
    return NULL;
}

static void tt_store(TranspositionTable *tt, uint64_t key, uint32_t move, int score, int depth, int flag){
    TTEntry *e=&tt->entries[key & (tt->size-1)];
    if(e->key==0 || e->depth <= depth){
        e->key=key; e->move=move; e->score=score; e->depth=depth; e->flag=flag; e->age=1;
    }
}

static int quiescence(SearchContext *ctx, Position *p, int alpha, int beta){
    ctx->nodes++;
    if((ctx->nodes & 1023)==0){
        if(now_ms()-ctx->start_ms >= ctx->time_limit_ms){ ctx->stop=1; }
    }
    if(ctx->stop) return 0;
    int standpat = ctx->evaluate(p,ctx->weights);
    if(standpat >= beta) return beta;
    if(standpat > alpha) alpha=standpat;

    MoveList ml;
    movegen_legal(p,&ml,1);
    int ttmove=0;
    TTEntry *te=tt_probe(&ctx->tt,p->hash);
    if(te) ttmove=te->move;
    search_order_moves_template(p,&ml,ttmove,ctx->weights);

    for(int i=0;i<ml.count;i++){
        Move *m=&ml.moves[i];
        Position tmp=*p;
        Undo u;
        make_move(&tmp,m,&u);
        int score = -quiescence(ctx,&tmp, -beta, -alpha);
        unmake_move(&tmp,m,&u);
        if(ctx->stop) return 0;
        if(score >= beta) return beta;
        if(score > alpha) alpha=score;
    }
    return alpha;
}

static int alphabeta(SearchContext *ctx, Position *p, int depth, int alpha, int beta, int ply){
    ctx->nodes++;
    if((ctx->nodes & 1023)==0){
        if(now_ms()-ctx->start_ms >= ctx->time_limit_ms){ ctx->stop=1; }
    }
    if(ctx->stop) return 0;
    if(depth<=0){
        return quiescence(ctx,p,alpha,beta);
    }
    /* terminal/repetition/draw */
    MoveList ml;
    movegen_legal(p,&ml,0);
    if(ml.count==0){
        if(in_check(p,p->side)) return -MATE_SCORE + ply;
        return 0; /* stalemate */
    }
    if(p->halfmove>=100) return 0;
    TTEntry *te=tt_probe(&ctx->tt,p->hash);
    int ttmove=0;
    if(te){
        ttmove=te->move;
        if(te->depth >= depth){
            int tscore=te->score;
            if(te->flag==0) return tscore;
            if(te->flag==1 && tscore >= beta) return tscore;
            if(te->flag==2 && tscore <= alpha) return tscore;
        }
    }
    search_order_moves_template(p,&ml,ttmove,ctx->weights);

    int alpha_orig=alpha;
    int best=-1000000000;
    int best_move_code=0;
    for(int i=0;i<ml.count;i++){
        Move *m=&ml.moves[i];
        Position tmp=*p;
        Undo u;
        make_move(&tmp,m,&u);
        int score = -alphabeta(ctx,&tmp, depth-1, -beta, -alpha, ply+1);
        unmake_move(&tmp,m,&u);
        if(ctx->stop) return 0;
        if(score > best){ best=score; best_move_code=packed_move(m); }
        if(score > alpha) alpha=score;
        if(alpha >= beta) break;
    }
    int flag=0;
    if(best <= alpha_orig) flag=2; /* upper bound */
    else if(best >= beta) flag=1; /* lower */
    else flag=0;
    tt_store(&ctx->tt,p->hash,best_move_code,best,depth,flag);
    return best;
}

int search_best_move(SearchContext *ctx, const Position *p, int time_limit_ms){
    ctx->time_limit_ms=time_limit_ms;
    ctx->start_ms=now_ms();
    ctx->nodes=0;
    ctx->stop=0;
    ctx->best_move=-1;
    ctx->best_score=0;
    ctx->depth_reached=0;
    Position root=*p;

    /* legal moves */
    MoveList rootmoves;
    movegen_legal(&root,&rootmoves,0);
    if(rootmoves.count==0) return -1;
    int maxdepth=64;
    for(int depth=1; depth<=maxdepth; depth++){
        if(now_ms()-ctx->start_ms >= ctx->time_limit_ms) break;
        int alpha=-INF_SCORE, beta=INF_SCORE;
        int best=-INF_SCORE;
        int localbestmove=-1;
        MoveList ml;
        movegen_legal(&root,&ml,0);
        int ttmove=0;
        TTEntry*te=tt_probe(&ctx->tt,root.hash);
        if(te) ttmove=te->move;
        search_order_moves_template(&root,&ml,ttmove,ctx->weights);
        int stop_this_depth=0;
        for(int i=0;i<ml.count;i++){
            Move*m=&ml.moves[i];
            Position tmp=root;
            Undo u;
            make_move(&tmp,m,&u);
            int score=-alphabeta(ctx,&tmp,depth-1,-beta,-alpha,1);
            unmake_move(&tmp,m,&u);
            if(ctx->stop){ stop_this_depth=1; break; }
            if(score > best){ best=score; localbestmove=m->from*64+m->to + (m->promo<<12); }
            if(score > alpha) alpha=score;
            if(alpha >= beta) break;
        }
        if(!stop_this_depth){
            ctx->best_score=best;
            ctx->depth_reached=depth;
            if(localbestmove!=-1) ctx->best_move=localbestmove;
            /* update tt root optional */
        }
        if(ctx->stop) break;
    }
    if(ctx->best_move==-1){
        /* fallback first legal */
        MoveList ml; movegen_legal(&root,&ml,0);
        ctx->best_move=ml.moves[0].from*64+ml.moves[0].to + (ml.moves[0].promo<<12);
    }
    return ctx->best_move;
}
