#ifndef POSITION_H
#define POSITION_H

#include "types.h"

typedef struct {
    Bitboard pieces[2][6];
    Bitboard occ[2];
    Bitboard occ_all;
    int side;
    int castling;
    int ep_square;
    int halfmove;
    int fullmove;
    uint64_t hash;
} Position;

typedef struct {
    int from, to, promo;
    int moving_piece;
    int captured_piece; /* 0..11 or -1 */
    int capture_square;
    int is_castle;
    int is_ep;
    int prev_castling;
    int prev_ep;
    int prev_halfmove;
    int prev_side;
    int prev_fullmove;
    uint64_t prev_hash;
} Undo;

Position position_start(void);
int position_from_fen(Position *p, const char *fen);
int position_to_fen(const Position *p, char *buf, int n);
void position_reset(Position *p);

int make_move(Position *p, const Move *m, Undo *u);
void unmake_move(Position *p, const Move *m, const Undo *u);

int square_attacked(const Position *p, int sq, Color attacker);
int in_check(const Position *p, Color side);
int is_legal(const Position *p, const Move *m);

int movegen_legal(const Position *p, MoveList *ml, int captures_only);
int movegen_pseudo(const Position *p, MoveList *ml, int captures_only);

uint64_t position_hash(const Position *p);
int has_insufficient_material(const Position *p);
int is_repetition(const Position *p, const Position *history[], int hist_count);
int is_terminal(const Position *p);
uint64_t perft_position(Position *p, int depth);

#endif
