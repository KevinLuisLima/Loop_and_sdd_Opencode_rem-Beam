#ifndef BITBOARD_H
#define BITBOARD_H
#include "types.h"
#include <stdbool.h>

extern const Bitboard FILE_MASKS[8];
extern const Bitboard RANK_MASKS[8];
extern const Bitboard SQUARE_BB[64];

void bitboard_init(void); /* no-op compatibility */

int popcount64(Bitboard b);
int lsb_index(Bitboard b); /* index of lowest set bit, or -1 */
Bitboard attacks_knight(int sq);
Bitboard attacks_king(int sq);
Bitboard attacks_pawn(int sq, Color c); /* squares attacked by pawn */

#endif
