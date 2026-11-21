#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    PT_PAWN = 0,
    PT_KNIGHT,
    PT_BISHOP,
    PT_ROOK,
    PT_QUEEN,
    PT_KING,
    PT_NONE
} PieceType;

#define COLOR_WHITE 0
#define COLOR_BLACK 1

#define SQUARE_INVALID (-1)

typedef struct {
    uint64_t bb[2][6];
    int side;
    uint8_t castle;
    int ep_square;
    int halfmove;
    int fullmove;
} Position;

typedef struct {
    int from;
    int to;
    int promo;
    int captured;
    int captured_color;
    uint8_t prev_castle;
    int prev_ep;
    int prev_halfmove;
    int prev_fullmove;
    int moved_piece;
    int moved_color;
    bool was_ep;
    bool was_castle;
    bool was_promo;
} Undo;

void pos_init_startpos(Position *p);
int pos_from_fen(Position *p, const char *fen);
int pos_to_fen(const Position *p, char *buf, size_t bufsz);
bool pos_square_ok(int sq);
int pos_piece_at(const Position *p, int sq, int *color);
uint64_t pos_occupancy(const Position *p, int color);
uint64_t pos_all_occupancy(const Position *p);

void pos_make_move(Position *p, int from, int to, int promo, Undo *u);
void pos_unmake_move(Position *p, const Undo *u);

int pos_side_to_move(const Position *p);
bool pos_in_check(const Position *p, int color);
bool pos_has_legal_moves(Position *p);
int pos_game_result(const Position *p);

#endif
BOARD_H