#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stddef.h>

#define WHITE 0
#define BLACK 1

#define PAWN 0
#define KNIGHT 1
#define BISHOP 2
#define ROOK 3
#define QUEEN 4
#define KING 5
#define PIECE_NO 6

typedef int Color;
typedef int Square;

typedef uint32_t Move;

#define MOVE_FROM(m) ((int)((m) & 0x3F))
#define MOVE_TO(m) ((int)(((m) >> 6) & 0x3F))
#define MOVE_PROMO(m) ((int)(((m) >> 12) & 0x7))
#define MOVE_FLAGS(m) ((int)(((m) >> 15) & 0x7))

#define MOVE_MAKE(f,t,p,fl) (((Move)(f)) | ((Move)(t) << 6) | ((Move)(p) << 12) | ((Move)(fl) << 15))

#define FLAG_NONE 0
#define FLAG_CAPTURE 1
#define FLAG_CASTLE 2
#define FLAG_EP 3
#define FLAG_PROMO 4

typedef struct {
    int captured;
    int capture_sq;
    int prev_castle;
    int prev_ep;
    int prev_halfmove;
    int prev_fullmove;
    uint64_t prev_hash;
    int moved_piece;
    int promo;
} Undo;

typedef struct {
    uint64_t pieces[2][PIECE_NO];
    uint64_t occ[3];
    int side;
    int castle;
    int ep;
    int halfmove;
    int fullmove;
    uint64_t hash;
    uint64_t hash_history[1024];
    int hash_count;
} Board;

typedef struct Weights {
    int version;
    int count;
    float vals[1024];
} Weights;

typedef struct SearchCtx {
    Board root;
    const Weights *weights;
    uint64_t nodes;
    long start_ms;
    long limit_ms;
    int stop;
    int max_depth;
    int depth_reached;
    void *tt;
    size_t tt_entries;
    Move killers[64][2];
    int history[2][64][64];
    int last_score;
} SearchCtx;

#endif
