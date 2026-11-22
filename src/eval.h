#ifndef EVAL_H
#define EVAL_H
#include "position.h"

#define FEATURE_COUNT 392

int eval_load_weights(const char *path, float **out, char *err, size_t errsz);
int eval_score(Position *pos, const float *w);
void eval_features(Position *pos, float *feat); /* length FEATURE_COUNT */

#endif