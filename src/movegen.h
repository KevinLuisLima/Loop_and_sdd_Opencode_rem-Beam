#ifndef MOVEGEN_H
#define MOVEGEN_H
#include "board.h"
#include "common.h"
void generateLegalMoves(Board *b, MoveList *ml);
int isInCheck(const Board *b, int color);
#endif
