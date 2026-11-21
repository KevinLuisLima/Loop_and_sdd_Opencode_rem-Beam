#include "position.h"
#include "search.h"
#include "eval.h"
#include "movegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void){ fprintf(stderr,"usage: chess options\n --fen STR\n --weights FILE\n --time MS\n --depth N\n --perft N\n"); }

int main(int argc, char **argv){
  const char *fen=NULL;
  const char *wpath=NULL;
  int time_ms=2000;
  int depth=8;
  int perft_depth=-1;
  for(int i=1;i<argc;i++){
    if(!strcmp(argv[i],"--fen") && i+1<argc){ fen=argv[++i]; }
    else if(!strcmp(argv[i],"--weights") && i+1<argc){ wpath=argv[++i]; }
    else if(!strcmp(argv[i],"--time") && i+1<argc){ time_ms=atoi(argv[++i]); }
    else if(!strcmp(argv[i],"--depth") && i+1<argc){ depth=atoi(argv[++i]); }
    else if(!strcmp(argv[i],"--perft") && i+1<argc){ perft_depth=atoi(argv[++i]); }
    else { usage(); return 1; }
  }
  Position pos;
  if(fen){ if(pos_from_fen(&pos,fen)!=0){ fprintf(stderr,"invalid fen\n"); return 2; } }
  else pos_init_startpos(&pos);

  if(perft_depth>=0){
    uint64_t n=perft(&pos,perft_depth);
    printf("%llu\n",(unsigned long long)n);
    return 0;
  }

  Weights *w=NULL;
  char err[256];
  if(wpath){
    w=weights_load(wpath,err,sizeof(err));
    if(!w){ fprintf(stderr,"%s\n",err); return 3; }
  } else {
    w=weights_load("weights/default.bin",err,sizeof(err));
    if(!w){ w=weights_create_default(); }
  }

  SearchContext ctx;
  search_context_init(&ctx,w,1,depth,time_ms);
  Move m=search_best_move(&ctx,&pos);
  if(m==0){
    fprintf(stderr,"no legal move\n");
    search_context_free(&ctx); weights_free(w); return 4;
  }
  char buf[8];
  snprintf(buf,sizeof(buf),"%c%c%c%c", 'a'+FILE(MOVE_FROM(m)), '1'+RANK(MOVE_FROM(m)), 'a'+FILE(MOVE_TO(m)), '1'+RANK(MOVE_TO(m)));
  if(MOVE_PROM(m)!=0){
    char p='q';
    int prom=MOVE_PROM(m);
    if(prom==PT_ROOK) p='r'; else if(prom==PT_BISHOP) p='b'; else if(prom==PT_KNIGHT) p='n';
    printf("%s%c\n",buf,p);
  } else {
    printf("%s\n",buf);
  }
  search_context_free(&ctx);
  weights_free(w);
  return 0;
}
