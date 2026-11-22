#ifndef EVAL_H
#define EVAL_H

#include "position.h"
#include "weights.h"

/* weight indices */
#define W_MATERIAL_PAWN 0
#define W_MATERIAL_KNIGHT 1
#define W_MATERIAL_BISHOP 2
#define W_MATERIAL_ROOK 3
#define W_MATERIAL_QUEEN 4
#define W_PST_BASE (5) /* 6 pieces * 64 = 384 -> 5..388 */
#define W_TERM_DOUBLED (5 + 6*64)
#define W_TERM_ISOLATED (6 + 6*64)
#define W_TERM_MOBILITY (7 + 6*64)
/* total 8 + 384 = 392 */

int eval_position(const Position *p, const Weights *w);

#endif