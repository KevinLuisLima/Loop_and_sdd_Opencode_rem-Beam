#ifndef BITBOARD_H
#define BITBOARD_H
#include <stdint.h>
#include <stddef.h>

uint64_t bb_set(int sq);
int bb_popcount(uint64_t x);
int bb_lsb(uint64_t x);
void bb_init(void);
uint64_t bb_pawn_attacks(int side, int sq);
uint64_t bb_knight(int sq);
uint64_t bb_king(int sq);
uint64_t bb_rook(int sq, uint64_t occ);
uint64_t bb_bishop(int sq, uint64_t occ);
uint64_t bb_queen(int sq, uint64_t occ);
/* attack mask tables computed via loop, but arrays precomputed */

#endif