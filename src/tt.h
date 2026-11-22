#ifndef TT_H
#define TT_H

#include "types.h"
#include "moves.h"

#define TT_BITS 16
#define TT_SIZE (1<<TT_BITS)

typedef struct {
    uint64_t key;
    int depth;
    int score;
    int flags; /* 0 exact 1 lower 2 upper */
    Move move;
} TtEntry;

typedef struct {
    TtEntry entries[TT_SIZE];
} TransTable;

void tt_clear(TransTable *tt);
TtEntry *tt_probe(TransTable *tt, uint64_t key);
void tt_store(TransTable *tt, uint64_t key, int depth, int score, int flags, Move move);

#endif