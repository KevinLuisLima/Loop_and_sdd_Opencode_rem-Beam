#ifndef SEARCH_H
#define SEARCH_H
#include "position.h"
#include "eval.h"
#include "types.h"
#include "movegen.h"
#include <stdbool.h>

typedef int (*EvalFunc)(const Position*, const Weights*);
typedef void (*OrderFunc)(MoveList*, const Position*);

typedef struct {
  const Weights *weights;
  EvalFunc eval;
  OrderFunc order;
  /* TT */
  struct TT *tt;
  /* limits */
  int max_depth;
  int max_time_ms; /* wall */
  long nodes;
  int stopped;
  /* start time */
  long start_ms;
  /* best move */
  Move best_move;
  int best_score;
  /* history? none */
} SearchContext;

void search_context_init(SearchContext *ctx, const Weights *w, int tt_mb, int max_depth, int max_time_ms);
void search_context_free(SearchContext *ctx);

/* Template method skeleton */
Move search_best_move(SearchContext *ctx, Position *pos);

/* perft */
uint64_t perft(Position *pos, int depth);

/* time */
long now_ms(void);

#endif
