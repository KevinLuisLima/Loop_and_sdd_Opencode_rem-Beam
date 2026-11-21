#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "eval.h"

int search_best_move(Position *pos, const WeightSet *ws, int max_depth, int time_ms, int *out_score);

#endif
SEARCH_H