#ifndef WEIGHTS_H
#define WEIGHTS_H
#include "types.h"
#include <stdio.h>
int weights_load(const char *path, Weights *w, char *err, size_t errsz);
int weights_save(const char *path, const Weights *w, char *err, size_t errsz);
void weights_default(Weights *w);
#endif
