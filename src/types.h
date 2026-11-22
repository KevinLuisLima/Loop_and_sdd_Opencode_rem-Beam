#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdlib.h>

typedef uint64_t Bitboard;

#define EMPTY 0
#define WP 1
#define WN 2
#define WB 3
#define WR 4
#define WQ 5
#define WK 6
#define BP 7
#define BN 8
#define BB 9
#define BR 10
#define BQ 11
#define BK 12

#define COLOR_WHITE 0
#define COLOR_BLACK 1

static inline int piece_color(int p){ return (p > WK) ? COLOR_BLACK : COLOR_WHITE; }
static inline int piece_type(int p){ return (p > WK) ? (p - WK) : p; }

#define SQ(a,b) ((a)*8 + (b))

#define MATE_SCORE 100000
#define INF_SCORE 1000000

#define MOVE_FROM(m) ((m) & 63)
#define MOVE_TO(m) (((m) >> 6) & 63)
#define MOVE_PROMO(m) (((m) >> 12) & 7)
#define MOVE_FLAGS(m) (((m) >> 15) & 7)

/* flags */
#define MF_QUIET 0
#define MF_CAPTURE 1
#define MF_DOUBLE 2
#define MF_CASTLE 3
#define MF_EP 4
#define MF_PROMO 5

static inline int make_move_int(int from, int to, int promo, int flags){
    return from | (to << 6) | (promo << 12) | (flags << 15);
}

#endif