#ifndef EVAL_H
#define EVAL_H
#include "position.h"
#include "types.h"
#include <stddef.h>

#define WEIGHTS_VERSION 1
#define WEIGHTS_DTYPE_FLOAT32 0
#define WEIGHT_COUNT 395  /* material 5 + pst 6*64 + terms 6? 5+384+6=395 */

typedef struct {
  int n;
  float *w; /* owned */
} Weights;

/* indices */
#define E_PAWN 0
#define E_KNIGHT 1
#define E_BISHOP 2
#define E_ROOK 3
#define E_QUEEN 4
#define E_PST (5) /* pieceType(0..5?) *64 + sq */
/* piece type index for PST: PT_PAWN..PT_KING -> 0..5 */
#define E_MOBILITY (5 + 6*64)     /* +1 */
#define E_DOUBLED_PAWN (E_MOBILITY+1)
#define E_ISOLATED_PAWN (E_DOUBLED_PAWN+1)
#define E_PASSED_PAWN (E_ISOLATED_PAWN+1)
#define E_KING_SAFETY (E_PASSED_PAWN+1)
#define E_TEMPO (E_KING_SAFETY+1)

Weights *weights_create_default(void);
Weights *weights_load(const char *path, char *err, size_t errsz);
void weights_free(Weights *w);
int evaluate(const Position *pos, const Weights *w); /* centipawns relative side-to-move */

/* san mirror index */
int mirror_sq(int sq);

#endif
