#ifndef BOARD_H
#define BOARD_H

#include "common.h"

#define CASTLE_WK 1
#define CASTLE_WQ 2
#define CASTLE_BK 4
#define CASTLE_BQ 8

typedef struct {
    Move move;
    int captured; /* piece type or -1 */
    int capturedSq;
    int prevCastling;
    int prevEp;
    int prevHalfmove;
    int prevFullmove;
    int prevSide;
} Undo;

typedef struct {
    Bitboard bb[2][6];
    int pieceOn[64]; /* -1 or color*6+type? we'll use (color<<4)|type, or encode type+color. Use int: -1 empty, else color*6+piece */
    int side;
    int castling;
    int epSquare;
    int halfmove;
    int fullmove;
    Undo history[1024];
    int histCount;
} Board;

void boardInit(Board *b);
void boardSetStartpos(Board *b);
int boardFromFEN(Board *b, const char *fen);
void boardToFEN(const Board *b, char *buf, size_t bufsz);
Bitboard boardOccupancy(const Board *b, int color); /* color -1 all */
Bitboard boardAllOccupancy(const Board *b);

int isSquareAttacked(const Board *b, int sq, int byColor);

void makeMove(Board *b, Move m);
void unmakeMove(Board *b);

/* piece helpers */
int pieceColor(int p);
int pieceType(int p);

#endif
