#ifndef EVAL_H
#define EVAL_H

#include "board.h"
#include <stdint.h>

#define EVAL_NUM_WEIGHTS 393

typedef struct {
    int version;
    int num_weights;
    float w[EVAL_NUM_WEIGHTS];
    int loaded;
} WeightSet;

int weights_load(WeightSet *ws, const char *path);
void weights_defaults(WeightSet *ws);

int eval_position(const Position *p, const WeightSet *ws);

#endif
EVAL_H