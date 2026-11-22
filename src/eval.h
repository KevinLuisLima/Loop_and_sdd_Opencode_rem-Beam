#ifndef EVAL_H
#define EVAL_H

#include "position.h"
#include "types.h"

#define EVAL_WEIGHT_COUNT 1024

typedef struct { float w[EVAL_WEIGHT_COUNT]; int iw[EVAL_WEIGHT_COUNT]; } EvalWeights;

void eval_weights_default(EvalWeights *ew);
int eval_load_weights(EvalWeights *ew, const char *path);

int evaluate_position(const Position *p, const EvalWeights *ew);

#endif
