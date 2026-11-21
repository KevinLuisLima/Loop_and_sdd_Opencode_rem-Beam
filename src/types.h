#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>

typedef uint64_t Bitboard;
typedef int Color;
typedef int PieceType;

#define COLOR_WHITE 0
#define COLOR_BLACK 1

#define PT_NONE 0
#define PT_PAWN 1
#define PT_KNIGHT 2
#define PT_BISHOP 3
#define PT_ROOK 4
#define PT_QUEEN 5
#define PT_KING 6

#define EMPTY 0
#define W_PAWN 1
#define W_KNIGHT 2
#define W_BISHOP 3
#define W_ROOK 4
#define W_QUEEN 5
#define W_KING 6
#define B_PAWN 7
#define B_KNIGHT 8
#define B_BISHOP 9
#define B_ROOK 10
#define B_QUEEN 11
#define B_KING 12

static inline Color piece_color(int p){ return p >= B_PAWN ? COLOR_BLACK : COLOR_WHITE; }
static inline int piece_type(int p){ if (p==EMPTY) return PT_NONE; if (p<=W_KING) return p; return p-6; }

#define FILE(x) ((x)&7)
#define RANK(x) ((x)>>3)
#define SQ_RC(r,f) (((r)<<3)|(f))

#endif
