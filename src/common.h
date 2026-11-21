#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>

typedef uint64_t Bitboard;

#define SQ_A1 0
#define SQ_H1 7
#define SQ_A8 56
#define SQ_H8 63

static inline int squareOf(int file, int rank) { return rank*8 + file; }
static inline int sqFile(int sq) { return sq & 7; }
static inline int sqRank(int sq) { return sq >> 3; }

#define MAX_MOVES 256

typedef struct {
    int from;
    int to;
    int promo; /* 0 none, 1 N,2 B,3 R,4 Q */
    int flags; /* castling etc handled by from/to */
} Move;

typedef struct {
    Move moves[MAX_MOVES];
    int count;
} MoveList;

static inline int popcount64(uint64_t x) {
#ifdef __POPCNT__
    return __builtin_popcountll(x);
#else
    return __builtin_popcountll(x);
#endif
}

/* piece types */
enum PieceType { PT_PAWN=0, PT_KNIGHT=1, PT_BISHOP=2, PT_ROOK=3, PT_QUEEN=4, PT_KING=5, PT_NONE=6 };
enum Color { WHITE=0, BLACK=1 };

/* debug assert for square range */
static inline void assert_square(int sq) {
    (void)sq;
    assert(sq >= 0 && sq < 64);
}

#endif /* COMMON_H */
