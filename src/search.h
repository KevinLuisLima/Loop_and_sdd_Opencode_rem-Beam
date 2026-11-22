#ifndef SEARCH_H
#define SEARCH_H
#include "types.h"
#include "board.h"
#include "eval.h"
void search_init_ctx(SearchCtx *ctx, const Weights *w);
void search_free_ctx(SearchCtx *ctx);
int search_bestmove(Board *root, SearchCtx *ctx, int max_depth, int time_ms, Move *bestmove, int *depth_out);
uint64_t search_nodes(SearchCtx *ctx);
#endif
