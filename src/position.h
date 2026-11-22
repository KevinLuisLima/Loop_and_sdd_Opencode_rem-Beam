#ifndef POSITION_H
#define POSITION_H

#include "types.h"
#include <stddef.h>
#include "moves.h"

typedef struct {
    Bitboard pieces[13];
    Bitboard occ[3];
    int side; /* 0 white */
    int castling;
    int ep;
    int halfmove;
    int fullmove;
    uint64_t hash;
    int castling_sq_set; /* unused */
} Position;

void pos_init_startpos(Position *p);
int pos_from_fen(Position *p, const char *fen);
void pos_to_fen(const Position *p, char *buf, size_t sz);
void pos_copy(Position *dst, const Position *src);

/* arrays of attack bitboards precomputed */
void attacks_init(void);
Bitboard knight_attacks(int sq);
Bitboard king_attacks(int sq);
Bitboard pawn_attacks(int sq, int color); /* color 0 white attacks upward? squares a pawn of color attacks */
Bitboard pawn_moves_quiet(int sq, int color);
Bitboard rook_attacks(int sq, Bitboard occ);
Bitboard bishop_attacks(int sq, Bitboard occ);
Bitboard queen_attacks(int sq, Bitboard occ);

int is_square_attacked(const Position *p, int sq, int byColor);
int in_check(const Position *p, int color);

/* undo info */
typedef struct {
    Move move;
    int captured;
    int prev_castling;
    int prev_ep;
    int prev_halfmove;
    uint64_t prev_hash;
    int rook_from;
    int rook_to;
    Bitboard prev_occ_all;
} Undo;

void make_move(Position *p, Move m, Undo *u);
void undo_move(Position *p, Undo *u);

/* result type */
typedef struct { int from,to,piece; } MoveInfo;

/* helper */
int square_piece(const Position *p, int sq);

/* detection draws / terminal */
int insufficient_material(const Position *p);

#endif