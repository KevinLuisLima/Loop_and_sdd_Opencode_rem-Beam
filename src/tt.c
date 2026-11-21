#include "tt.h"
#include <stdlib.h>
#include <string.h>

void tt_init(TT *tt, int mb){
  if(!tt) return;
  size_t bytes = (size_t)mb*1024*1024;
  if(bytes < sizeof(TTEntry)) bytes=sizeof(TTEntry);
  size_t n = bytes / sizeof(TTEntry);
  /* power of two */
  size_t s=1; while(s<n) s<<=1;
  tt->entries = calloc(s, sizeof(TTEntry));
  tt->size=s; tt->mask=s-1;
}
void tt_free(TT *tt){
  if(!tt) return;
  free(tt->entries); tt->entries=NULL; tt->size=0; tt->mask=0;
}
TTEntry *tt_probe(TT *tt, uint64_t key){
  if(!tt||!tt->entries) return NULL;
  TTEntry *e = &tt->entries[key & tt->mask];
  if(e->key==key) return e;
  return NULL;
}
void tt_store(TT *tt, uint64_t key, int depth, int score, Move move, int flag, int alpha, int beta){
  (void)alpha;(void)beta;
  if(!tt||!tt->entries) return;
  TTEntry *e = &tt->entries[key & tt->mask];
  /* replace if deeper or empty */
  if(e->key==0 || depth >= e->depth){
    e->key=key; e->depth=depth; e->score=score; e->move=move; e->flag=flag;
  }
}
