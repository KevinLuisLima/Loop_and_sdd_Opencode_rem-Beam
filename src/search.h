#ifndef SEARCH_H
#define SEARCH_H
#include "board.h"
#include "weights.h"
#include "common.h"

typedef struct { int nodes; } SearchStats;

Move searchBestMove(Board *b, const Weights *w, int timeMs, SearchStats *stats);
int searchAtDepth(Board *b, const Weights *w, int depth, Move *best);
int evaluateForSearch(const Board *b, const Weights *w);

#endif