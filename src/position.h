#ifndef POSITION_H
#define POSITION_H

#include "types.h"
#include "bitboard.h"
#include <stdint.h>

typedef struct {
    int captured;
    int capture_sq;
    int castling;
    int ep;
    int halfmove;
    uint64_t hash;
    Move move;
    int side;
    int fullmove;
    int promoted_piece;
    int piece;
} Undo;

typedef struct Position {
    uint64_t pieces[2][6];
    uint64_t occ[2];
    uint64_t occ_all;
    signed char board[64];

    int side;
    int castling;
    int ep;
    int halfmove;
    int fullmove;

    uint64_t hash;

    uint64_t hist[HISTORY_SIZE];
    int hist_count;

    Undo undo[MAX_PLY + 64];
    int undo_count;
    int ply;
} Position;

void position_init(Position *pos);
int position_from_fen(Position *pos, const char *fen, char *err, size_t errsz);
void position_to_fen(Position *pos, char *buf, size_t sz);
void position_reset_history(Position *pos);

int make_move(Position *pos, Move m, Undo *undo_out);
void unmake_move(Position *pos, Undo *u);

int generate_pseudo_moves(Position *pos, Move *moves);
int generate_legal_moves(Position *pos, Move *moves);
int move_is_legal(Position *pos, Move m);

uint64_t attacks_to(Position *pos, int sq, int side);
int in_check(Position *pos, int side);

int is_terminal(Position *pos, int *result); /* 1 if terminal, result: 1 win side? We'll define: returns game state */

int insufficient_material(Position *pos);

uint64_t position_hash(Position *pos);

int code_color(int code);
int code_type_idx(int code);

#endif