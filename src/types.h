#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stddef.h>

#define EMPTY 0
#define WKING 1
#define WPAWN 2
#define WKNIGHT 3
#define WBISHOP 4
#define WROOK 5
#define WQUEEN 6
#define BKING 7
#define BPAWN 8
#define BKNIGHT 9
#define BBISHOP 10
#define BROOK 11
#define BQUEEN 12

#define IS_WHITE(p) ((p) >= WKING && (p) <= WQUEEN)
#define IS_BLACK(p) ((p) >= BKING && (p) <= BQUEEN)
#define PIECE_TYPE(p) ((((p)-1)%6)+1)
#define PIECE_COLOR(p) ((p) <= WQUEEN ? 0 : 1)

#define WT 0
#define BT 1

#define MAX_MOVES 256
#define MAX_PLY 256
#define HISTORY_SIZE 1024

typedef uint16_t Move;

#define MOVE_FROM(m)  ((m) & 0x3F)
#define MOVE_TO(m)    (((m)>>6) & 0x3F)
#define MOVE_PROM(m)  (((m)>>12) & 0x7)

typedef enum {
    M_NORMAL = 0,
    M_CAPTURE = 1,
    M_EP = 2,
    M_CASTLE_K = 3,
    M_CASTLE_Q = 4,
    M_PROM_Q = 5,
    M_PROM_R = 6,
    M_PROM_B = 7,
    M_PROM_N = 8
} MoveType;

typedef struct {
    Move move;
    MoveType type;
    int piece;
    int captured;
    int from_sq;
    int to_sq;
    int prom;
} MoveInfo;

typedef struct {
    uint64_t key;
    int score;
    int depth;
    int flag;
    Move best;
} TTEntry;

#endif