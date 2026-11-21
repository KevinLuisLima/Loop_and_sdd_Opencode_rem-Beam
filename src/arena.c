#include "position.h"
#include "search.h"
#include "eval.h"
#include "movegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Weights *loadw(const char *p){ char err[256]; Weights *w=weights_load(p,err,sizeof(err)); if(!w){ fprintf(stderr,"%s\n",err); exit(3);} return w; }


int main(int argc, char**argv){
  if(argc<7){ fprintf(stderr,"usage: arena weightsA weightsB games depth time_ms [startfen] [judge_depth]\n"); return 1;}
  const char *wa=argv[1], *wb=argv[2];
  int games=atoi(argv[3]);
  int depth=atoi(argv[4]);
  int time_ms=atoi(argv[5]);
  const char *startfen = (argc>6)? argv[6] : NULL;
  int judge_depth = (argc>7)? atoi(argv[7]) : depth+2;
  int threshold = 50;

  Weights *W[2];
  W[0]=loadw(wa); W[1]=loadw(wb);

  int total_moves[2]={0,0};
  int total_subopt[2]={0,0};
  double score[2]={0,0};
  int total_games=0;
  for(int g=0; g<games; g++){
    Position pos;
    if(startfen) pos_from_fen(&pos,startfen); else pos_init_startpos(&pos);
    /* alternate colors: even game A white (0), odd A black (1) */
    int colorA = (g%2==0)?0:1; /* 0 means A is white? We'll map side index */
    /* player for side: side0 white, side1 black. A plays side colorA? Let's set: if colorA==0 A white. */
    int moves=0;
    int game_win_side=-1; /* -1 draw, 0 white win, 1 black win */
    /* subopt per player aggregate */
    while(1){
      MoveList ml; ml.count=0;
      generate_legal_moves(&pos,&ml);
      if(ml.count==0){
        if(side_in_check(&pos,pos.side)) game_win_side = pos.side ^ 1;
        break;
      }
      if(pos.halfmove>=100 || moves>=300){ game_win_side=-1; break; }
      /* decide which model plays side pos.side */
      int side = pos.side; /* 0 white 1 black */
      /* map side to player (0 A,1 B) */
      int player = (side==colorA)?0:1;
      Weights *wpl=W[player];
      /* judge best score before move */
      SearchContext jctx;
      search_context_init(&jctx,wpl,1,judge_depth, (time_ms*2>0)? time_ms*2 : time_ms);
      /* but eval uses w; judge using same weights but deeper */
      /* score best */
      Position jpos=pos;
      Move bestm = search_best_move(&jctx,&jpos);
      int bestscore = jctx.best_score;
      search_context_free(&jctx);
      /* player move */
      SearchContext pctx;
      search_context_init(&pctx,wpl,1,depth,time_ms);
      Position ppos=pos;
      Move m = search_best_move(&pctx,&ppos);
      int pscore = pctx.best_score;
      search_context_free(&pctx);
      if(m==0){ game_win_side = 1-side; break; }
      /* suboptimal? compare bestscore vs pscore for played move */
      /* Need score of actual move. Approx: if played move != best move from deeper judge, recompute its score */
      /* We'll run root negamax depth-1 for actual move */
      /* Simpler: if bestm != m, compute score difference by making move m at depth judge */
      int move_score = pscore;
      if(bestm != m && bestm!=0){
        /* make m and evaluate with judge search depth-1 */
        Position c=pos; Undo u; make_move(&c,m,&u);
        SearchContext mc; search_context_init(&mc,wpl,1,judge_depth-1,time_ms);
        /* We can't easily get score of played move except negamax style. Call search_best_move on child? That returns opponent perspective.
           Approx difference using best score - (-child_best) */
        Position cc=c;
        (void)search_best_move(&mc,&cc);
        int child_best = mc.best_score;
        search_context_free(&mc);
        /* score for played move (side perspective) = -child_best */
        move_score = -child_best;
      }
      /* update subopt if bestscore - move_score > threshold */
      if(bestm!=0 && bestscore - move_score > threshold){
        total_subopt[player]++;
      }
      total_moves[player]++;
      /* apply move */
      Undo u;
      make_move(&pos,m,&u);
      moves++;
    }
    /* determine winner player mapping */
    /* winner encoded as side index or -1. Let's adjust: we set winner=-1 for draw; for checkmate we didn't set side. Rework below impossible. Use variable win_side. */
    /* To keep score, use winner as side (0 white win) only if set. We'll patch: replace with better handling done via turns? 
       Given complexity, track winner by counting moves parity: if no moves and side in check, winner side = opponent. Implement here. */
    /* We didn't track; set after loop impossible. Let's instead handle checkmate inside loop. We'll restructure: when generate_legal_moves 0, set:
          win_side = pos.side ^ 1  (opponent wins) if side_in_check else -1. Then break. 
       Need edit; final build will catch wrong. We'll do that now. */
    /* placeholder no effect */
    (void)moves;
    if(game_win_side==-1){
      score[0]+=0.5; score[1]+=0.5;
    } else {
      int winning_player = (game_win_side==colorA)?0:1;
      score[winning_player]+=1.0;
    }
    total_games++;
  }
  /* print aggregate */
  double g = games>0? games : 1;
  printf("games=%d\n", total_games);
  printf("scoreA=%.6f\n", score[0]/g);
  printf("scoreB=%.6f\n", score[1]/g);
  printf("movesA=%d movesB=%d\n", total_moves[0], total_moves[1]);
  printf("suboptA=%d suboptB=%d\n", total_subopt[0], total_subopt[1]);
  if(total_moves[0]>0) printf("subopt_rateA=%.6f\n", (double)total_subopt[0]/total_moves[0]);
  if(total_moves[1]>0) printf("subopt_rateB=%.6f\n", (double)total_subopt[1]/total_moves[1]);
  weights_free(W[0]); weights_free(W[1]);
  return 0;
}
