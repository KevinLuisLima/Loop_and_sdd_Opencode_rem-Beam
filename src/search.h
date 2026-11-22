#ifndef SEARCH_H
#define SEARCH_H

#include "position.h"
#include "types.h"
#include "eval.h"
#include <stddef.h>

typedef struct {
    uint64_t key;
    uint32_t move;
    int score;
    int depth;
    unsigned char flag; /* 0 exact,1 lower,2 upper */
    unsigned char age;
} TTEntry;

typedef struct {
    TTEntry *entries;
    size_t size;
} TranspositionTable;

typedef struct {
    Position pos;
    EvalWeights *weights;
    TranspositionTable tt;
    int time_limit_ms;
    long start_ms;
    int nodes;
    int stop;
    int depth_reached;
    int best_move;
    int best_score;
    /* template hook pointers */
    int (*evaluate)(const Position*, const EvalWeights*);
    int (*score_move)(const Position*, const Move*, int tt_move, const EvalWeights*);
} SearchContext;

void search_init(SearchContext *ctx, EvalWeights *ew);
void search_free(SearchContext *ctx);
int search_best_move(SearchContext *ctx, const Position *p, int time_limit_ms);

/* template steps (search skeleton calls these) */
int search_eval_template(const Position *p, const EvalWeights *ew);
int search_order_moves_template(const Position *p, MoveList *ml, int tt_move, const EvalWeights *ew);

#endif
