#ifndef EVAL_H
#define EVAL_H
#include "board.h"

#define N_WEIGHTS 395
#define W_PAWN_MAT   0
#define W_KNIGHT_MAT 1
#define W_BISHOP_MAT 2
#define W_ROOK_MAT   3
#define W_QUEEN_MAT  4
#define W_PST        5
#define W_MOB        (W_PST + 6*64)
#define W_DOUBLED    (W_MOB + 1)
#define W_ISOLATED   (W_MOB + 2)
#define W_PASSED     (W_MOB + 3)
#define W_BISHOP_PAIR (W_MOB + 4)
#define W_KING_SAFETY (W_MOB + 5)

int load_weights(const char* path, double* w, int n);
int evaluate(const Position* p, const double* w);

#endif
