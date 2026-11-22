#ifndef SEARCH_H
#define SEARCH_H
#include "board.h"
#include <stdbool.h>

#define INF 1000000
#define MATE 900000

typedef struct {
    uint64_t key;
    int depth;
    int score;
    int flag; /* 0 exact 1 lower 2 upper */
    Move move;
    int valid;
} TTEntry;

typedef struct {
    TTEntry* entries;
    int size;
} TT;

typedef struct {
    Position* pos;
    const double* weights;
    TT tt;
    int timeBudgetMs;
    long t0;
    int stopped;
    uint64_t nodes;
    Move bestMove;
    int bestScore;
    int bestDepth;
} SearchContext;

void search_init(SearchContext* ctx, Position* p, const double* w, int timeMs);
void search_free(SearchContext* ctx);
Move search_bestmove(SearchContext* ctx);
uint64_t search_nodes(SearchContext* ctx);

#endif
