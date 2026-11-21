#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "board.h"

int movegen_pseudo(const Position *p, int *moves, int maxmoves);
int movegen_legal_moves(Position *p, int *moves, int maxmoves);
int movegen_count_legal(Position *p);

static const int MOVE_MAX = 256;

#endif
MOVEGEN_H