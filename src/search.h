#ifndef SEARCH_H
#define SEARCH_H

#include "position.h"
#include "weights.h"
#include "tt.h"

#define SEARCH_MAX_DEPTH 64
#define SEARCH_HISTORY 4096

typedef struct {
    TransTable tt;
    int nodes;
    int stop;
    double start_time;
    double time_limit; /* seconds */
    int history_count;
    uint64_t history[SEARCH_HISTORY];
    Move best_root_move;
    int best_root_score;
    int best_depth;
    /* for bestmove */
} SearchCtx;

void search_init(SearchCtx *ctx, double time_limit_seconds);
Move search_bestmove(SearchCtx *ctx, Position *root, Weights *w, int fixed_depth, double fixed_time_seconds);
/* fixed_depth>0 uses depth limit ignoring time; fixed_time_seconds>0 uses time. If both <=0 use ctx time_limit */
int search_perft(Position *p, int depth);

#endif