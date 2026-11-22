#ifndef EVAL_H
#define EVAL_H
#include "types.h"
#include "board.h"

#define NUM_WEIGHTS 393
#define W_MATERIAL 0
#define W_PST 5
#define W_MOBILITY 389
#define W_DOUBLED 390
#define W_ISOLATED 391
#define W_KING 392

int eval_position(const Board *b, const Weights *w);

#endif
