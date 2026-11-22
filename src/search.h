#ifndef SEARCH_H
#define SEARCH_H
#include "position.h"
#include "eval.h"
#include <stdint.h>

typedef struct {
    const float *weights;
    uint64_t nodes;
    int max_depth;
    int movetime_ms;
    long start_ms;
    int stopped;
    int best_score;
    Move best_move;
    int reached_depth;
    /* TT */
    TTEntry *tt;
    int tt_mask;
    /* ordering */
    Move killer[MAX_PLY+4][2];
    int history[64][64];
} SearchContext;

long now_ms(void);
SearchContext *search_create(const float *weights, int tt_entries);
void search_destroy(SearchContext *ctx);
Move search_best_move(SearchContext *ctx, Position *pos, int max_depth, int movetime_ms);

#endif