#ifndef TT_H
#define TT_H
#include "position.h"
#include "types.h"
#include <stdbool.h>

typedef struct TTEntry {
  uint64_t key;
  int depth;
  int score;
  Move move;
  int8_t flag; /* 0 exact,1 lower,2 upper */
} TTEntry;

typedef struct TT {
  TTEntry *entries;
  size_t size;
  size_t mask;
} TT;

void tt_init(TT *tt, int mb);
void tt_free(TT *tt);
TTEntry *tt_probe(TT *tt, uint64_t key);
void tt_store(TT *tt, uint64_t key, int depth, int score, Move move, int flag, int alpha, int beta);

#endif
