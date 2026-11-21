#ifndef MOVEGEN_H
#define MOVEGEN_H
#include "position.h"
#include "types.h"

typedef struct { Move moves[256]; int count; } MoveList;

void generate_pseudo_moves(const Position *p, MoveList *ml, bool captures_only);
/* legal moves filtering */
int generate_legal_moves(const Position *p, MoveList *ml);
Move make_move_from_uci(Position *p, const char *str); /* not here maybe */

#endif
