#ifndef WEIGHTS_H
#define WEIGHTS_H

#include <stddef.h>

#define WEIGHT_MAGIC 0x43425357 /* "WSBC"? */
#define WEIGHT_VERSION 1
#define WEIGHT_COUNT 392

typedef struct {
    int count;
    float *data;
} Weights;

int weights_load(Weights *w, const char *path);
void weights_free(Weights *w);
float weights_get(const Weights *w, int idx);
int weights_default_path(char *buf, size_t sz);

#endif