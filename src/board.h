#ifndef BOARD_H
#define BOARD_H

#include "types.h"

void board_init(Board *b);
int board_load_fen(Board *b, const char *fen);
void board_to_fen(const Board *b, char *buf, size_t sz);

int in_check(const Board *b, int side);
uint64_t attacks_to(const Board *b, int sq, int attacker_side);

void gen_moves(const Board *b, Move *moves, int *count);
void gen_captures(const Board *b, Move *moves, int *count);
int is_legal(Board *b, Move m);

int make_move(Board *b, Move m, Undo *u);
void unmake_move(Board *b, Move m, const Undo *u);

int game_over(const Board *b); /* 0 ongoing,1 mate,2 stalemate/draw */
int insufficient_material(const Board *b);
int repetition(const Board *b);

uint64_t hash_board(const Board *b);

void print_board(const Board *b);

#endif
