#ifndef WEIGHTS_H
#define WEIGHTS_H
#include <stdint.h>
#include <stdio.h>
#define EXPECTED_WEIGHTS 396
#define WEIGHTS_VERSION 1

typedef struct {
    float w[EXPECTED_WEIGHTS];
    int count;
} Weights;

int weightsLoad(const char *path, Weights *out, char *errbuf, size_t errsz);
int weightsValidate(const Weights *w);
#endif
