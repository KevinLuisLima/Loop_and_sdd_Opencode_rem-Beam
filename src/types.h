#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

#define MAX_MOVES 512
#define MATE_SCORE 100000
#define INF_SCORE 300000

typedef enum { COLOR_WHITE=0, COLOR_BLACK=1 } Color;
typedef enum { PIECE_PAWN=0, PIECE_KNIGHT=1, PIECE_BISHOP=2, PIECE_ROOK=3, PIECE_QUEEN=4, PIECE_KING=5, PIECE_NONE=6 } PieceType;

typedef uint64_t Bitboard;

typedef struct {
    int from;
    int to;
    int promo; /* PIECE_* or PIECE_NONE */
    int score;
} Move;

typedef struct { Move moves[MAX_MOVES]; int count; } MoveList;

#endif
