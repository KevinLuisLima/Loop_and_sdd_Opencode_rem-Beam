#include "search.h"
#include "tt.h"
#include "movegen.h"
#include "eval.h"
#include "position.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

long now_ms(void){
  struct timeval tv; gettimeofday(&tv,NULL);
  return (long)tv.tv_sec*1000L + tv.tv_usec/1000L;
}

#define INF 1000000
#define MATE 900000

static int piece_value(int pt){
  switch(pt){ case PT_PAWN: return 100; case PT_KNIGHT: return 320; case PT_BISHOP:return 330;
  case PT_ROOK:return 500; case PT_QUEEN:return 900; case PT_KING:return 0; default: return 0; }
}

static void default_order(MoveList *ml, const Position *p){
  /* compute sort keys */
  int keys[256];
  for(int i=0;i<ml->count;i++){
    Move m=ml->moves[i];
    int to=MOVE_TO(m), from=MOVE_FROM(m);
    int victim = (MOVE_FLAGS(m)&FLAG_EP)? PT_PAWN : (p->board[to]!=EMPTY? piece_type(p->board[to]) : PT_NONE);
    int attacker = piece_type(p->board[from]);
    int score=0;
    if(MOVE_FLAGS(m)&FLAG_CAPTURE) score += 10000 + piece_value(victim)*10 - piece_value(attacker);
    if(MOVE_PROM(m)!=0) score += 5000 + piece_value(MOVE_PROM(m));
    /* castling slight */
    if(MOVE_FLAGS(m)&FLAG_CASTLE) score += 10;
    keys[i]=score;
  }
  /* insertion sort descending */
  for(int i=1;i<ml->count;i++){
    Move mi=ml->moves[i]; int ki=keys[i]; int j=i-1;
    while(j>=0 && keys[j]<ki){ ml->moves[j+1]=ml->moves[j]; keys[j+1]=keys[j]; j--; }
    ml->moves[j+1]=mi; keys[j+1]=ki;
  }
}

void search_context_init(SearchContext *ctx, const Weights *w, int tt_mb, int max_depth, int max_time_ms){
  if(!ctx) return;
  ctx->weights=w;
  ctx->eval = (EvalFunc)evaluate;
  ctx->order = (OrderFunc)default_order;
  ctx->tt = malloc(sizeof(TT));
  tt_init(ctx->tt, tt_mb>0?tt_mb:1);
  ctx->max_depth = max_depth>0?max_depth:8;
  ctx->max_time_ms = max_time_ms>0?max_time_ms:1000;
  ctx->nodes=0;
  ctx->stopped=0;
  ctx->start_ms=0;
  ctx->best_move=0;
  ctx->best_score=0;
}

void search_context_free(SearchContext *ctx){
  if(!ctx) return;
  if(ctx->tt){ tt_free(ctx->tt); free(ctx->tt); ctx->tt=NULL; }
}

static int time_up(SearchContext *ctx){
  if(ctx->max_time_ms<=0) return 0;
  long now=now_ms();
  if(now - ctx->start_ms >= ctx->max_time_ms){ ctx->stopped=1; return 1; }
  return 0;
}

static int quiescence(SearchContext *ctx, Position *pos, int alpha, int beta, int ply){
  if(time_up(ctx)) return 0;
  int stand = ctx->eval(pos, ctx->weights);
  if(stand >= beta) return beta;
  if(stand > alpha) alpha = stand;
  MoveList ml; ml.count=0;
  generate_legal_moves(pos, &ml);
  /* keep captures only */
  MoveList caps; caps.count=0;
  for(int i=0;i<ml.count;i++) if(MOVE_FLAGS(ml.moves[i]) & FLAG_CAPTURE) caps.moves[caps.count++]=ml.moves[i];
  ctx->order(&caps, pos);
  for(int i=0;i<caps.count;i++){
    Position c=*pos;
    Undo u;
    make_move(&c, caps.moves[i], &u);
    /* check legality already ensured by generate_legal; but king safety? generate_legal filters */
    int score = -quiescence(ctx, &c, -beta, -alpha, ply+1);
    unmake_move(&c, caps.moves[i], &u);
    ctx->nodes++;
    if(ctx->stopped) return 0;
    if(score >= beta) return beta;
    if(score > alpha) alpha = score;
  }
  return alpha;
}

static int negamax(SearchContext *ctx, Position *pos, int depth, int alpha, int beta, int ply){
  if(ctx->stopped) return 0;
  if(time_up(ctx)) return 0;
  /* node count check time every 4096 */
  if((ctx->nodes & 4095)==0) { if(time_up(ctx)) return 0; }
  uint64_t key=pos_hash(pos);
  /* mate distance not tracked; basic */
  TTEntry *te=tt_probe(ctx->tt, key);
  if(te && te->depth >= depth){
    if(te->flag==0) return te->score;
    if(te->flag==1 && te->score > alpha) alpha=te->score; /* lower */
    if(te->flag==2 && te->score < beta) beta=te->score; /* upper */
    if(alpha>=beta) return te->score;
  }

  MoveList ml; ml.count=0;
  generate_legal_moves(pos, &ml);
  if(ml.count==0){
    if(side_in_check(pos, pos->side)) return -MATE + ply;
    return 0; /* stalemate */
  }
  if(depth<=0) return quiescence(ctx, pos, alpha, beta, ply);

  ctx->order(&ml, pos);
  /* TT move first if available */
  if(te && te->move){
    for(int i=0;i<ml.count;i++){
      if(ml.moves[i]==te->move){ Move tmp=ml.moves[0]; ml.moves[0]=ml.moves[i]; ml.moves[i]=tmp; break; }
    }
  }

  int best=-INF;
  Move bestMove=ml.moves[0];
  int flag=2; /* upper */
  for(int i=0;i<ml.count;i++){
    Position c=*pos;
    Undo u;
    make_move(&c, ml.moves[i], &u);
    int score = -negamax(ctx, &c, depth-1, -beta, -alpha, ply+1);
    unmake_move(&c, ml.moves[i], &u);
    ctx->nodes++;
    if(ctx->stopped) return best== -INF?0:best;
    if(score > best){ best=score; bestMove=ml.moves[i]; }
    if(score > alpha){ alpha=score; flag=0; } /* exact if within */
    if(alpha >= beta){ flag=1; break; } /* lower bound */
  }
  tt_store(ctx->tt, key, depth, best, bestMove, flag, alpha, beta);
  return best;
}

uint64_t perft(Position *pos, int depth){
  if(depth==0) return 1ULL;
  MoveList ml; ml.count=0;
  generate_legal_moves(pos,&ml);
  if(depth==1) return (uint64_t)ml.count;
  uint64_t total=0;
  for(int i=0;i<ml.count;i++){
    Position c=*pos;
    Undo u;
    make_move(&c, ml.moves[i], &u);
    total += perft(&c, depth-1);
    unmake_move(&c, ml.moves[i], &u);
  }
  return total;
}

Move search_best_move(SearchContext *ctx, Position *pos){
  ctx->stopped=0;
  ctx->nodes=0;
  ctx->start_ms=now_ms();
  ctx->best_move=0; ctx->best_score=0;

  MoveList ml; ml.count=0;
  generate_legal_moves(pos, &ml);
  if(ml.count==0) return 0;
  if(ml.count==1){ ctx->best_move=ml.moves[0]; return ctx->best_move; }

  /* iterative deepening */
  for(int d=1; d<=ctx->max_depth; d++){
    ctx->nodes=0; /* reset per depth? keep total maybe; but time uses start */
    ctx->best_score = negamax(ctx, pos, d, -INF, INF, 0);
    /* best move from TT root */
    TTEntry *te=tt_probe(ctx->tt, pos_hash(pos));
    if(te && te->move) ctx->best_move=te->move;
    else if(ml.count>0) ctx->best_move=ml.moves[0];
    if(ctx->stopped) break;
    long elapsed = now_ms()-ctx->start_ms;
    if(ctx->max_time_ms>0 && elapsed >= (long)(ctx->max_time_ms*0.8)) break;
  }
  if(ctx->best_move==0 && ml.count>0) ctx->best_move=ml.moves[0];
  return ctx->best_move;
}
