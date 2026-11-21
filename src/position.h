#ifndef POSITION_H
#define POSITION_H
#include "types.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint32_t Move;

#define MOVE_FROM(m) ((m)&63)
#define MOVE_TO(m) (((m)>>6)&63)
#define MOVE_PROM(m) (((m)>>12)&7)
#define MOVE_FLAGS(m) (((m)>>15)&7)

#define FLAG_NONE 0
#define FLAG_CAPTURE 1
#define FLAG_EP 2
#define FLAG_CASTLE 4
#define FLAG_PROMO 8

/* combine: store capture flag separately? We'll set */

typedef struct {
  int board[64];
  Bitboard bb[13];
  Bitboard occ[2];
  Bitboard occ_all;
  Color side;
  int castling;
  int ep;
  int halfmove;
  int fullmove;
  uint64_t hash;
} Position;

typedef struct {
  int from,to,promoted,captured;
  int castling,ep,halfmove;
  uint64_t hash;
  int flags;
} Undo;

void pos_init_startpos(Position *p);
int pos_from_fen(Position *p, const char *fen);
int pos_to_fen(const Position *p, char *buf, size_t bufsz);
uint64_t pos_hash(const Position *p);

void make_move(Position *p, Move m, Undo *u);
void unmake_move(Position *p, Move m, const Undo *u);

bool is_square_attacked(const Position *p, int sq, Color by);
int side_in_check(const Position *p, Color side);

#endif
